#ifndef GBEES_H
#define GBEES_H

#include "BST.h"
/*==============================================================================
CLASS DEFINITIONS
==============================================================================*/
class Lorenz3D{ // Properties of the trajectory
  public:            
    int sigma;        
    int b;
    int r;
    int L;
};

class GBEES{       // HGBEES Class
  public:
    BST P;
    TreeNode* dead; 

    void Initialize_D(Grid G,Lorenz3D Lor);
    void Initialize_vuw(Grid G,Lorenz3D Lor, TreeNode* r);
    void Modify_pointset(Grid G, Lorenz3D Lor);
    TreeNode* create_neighbors(Grid G, TreeNode* r, double& prob_sum);
    TreeNode* delete_neighbors(Grid G, TreeNode* r);
    void RHS_P(Grid G,Lorenz3D Lor);
    void initial_f(Grid G, TreeNode* r); 
    void total_f(Grid G, TreeNode* r); 
    void calculating_K(Grid G, TreeNode* r);
    void update_prob(Grid G, TreeNode* r); 
    void Record_Data(std::string file_name, Grid G, TreeNode* r);
    void normalize_prob(TreeNode* r, double prob_sum);
};
/*==============================================================================
Non-member Function DEFINITIONS
==============================================================================*/
int RosenbergPair(int state[], int d, int m){
    int key; 
    if(d == 1){
        return key = state[0];
    }else{
        int new_state[d-1];
        for (int i = 0; i < d-1; i++){
            new_state[i] = state[i]; 
        }
        int new_m = *std::max_element(new_state, new_state + d-1);
        return key = RosenbergPair(new_state, d-1, new_m) + pow(m,d) + (m - state[d-1])*(pow(m+1, d-1) - pow(m,d-1)); 
    }
};

int state_conversion(std::array<int,DIM> state){
    int shift_state[DIM];
    for (int i = 0; i < DIM; i++){
        if(state[i]<0){
            shift_state[i] = -2*state[i]-1;
        }else{
            shift_state[i] = 2*state[i];
        }
    }
    int m = *std::max_element(shift_state, shift_state + DIM);
    int key = RosenbergPair(shift_state, DIM, m);
    return key;
};

double MC(double th){
    double phi;
    phi = std::max(0.0,std::min({(1+th)/2,2.0,2*th}));
    return phi;
};
/*==============================================================================
Member Function DEFINITIONS
==============================================================================*/
void GBEES::Initialize_D(Grid G,Lorenz3D Lor){
    Cell blank_c = {.prob = 0, .vp = {0}, .up = {0}, .wp = {0}, .vm = {0}, .um = {0}, .wm = {0}, .f = {0}, .active = 0, .K = 0};
    TreeNode* dead_node = new TreeNode(-1, blank_c); P.root = P.insertRecursive(P.root, dead_node);
    dead = dead_node; 

    std::array<int,DIM> current_state; int key; 
    for (int i = round((G.start[0]-2)/G.del); i <= round((G.start[0]+2)/G.del); i++){
        for (int j = round((G.start[1]-2)/G.del); j <= round((G.start[1]+2)/G.del); j++){
            for (int k = round((G.start[2]-2)/G.del); k <= round((G.start[2]+2)/G.del); k++){
                current_state = {i,j,k}; key = state_conversion(current_state); 
                double x = pow(i*G.del - G.start[0],2)+pow(j*G.del - G.start[1],2)+pow(k*G.del - G.start[2],2);
                Cell c = {.prob = exp(-4*x/2), .vp = {0}, .up = {0}, .wp = {0}, .vm = {0}, .um = {0}, .wm = {0}, .f = {0}, .state = current_state, .active = 0, .K = 0};
                TreeNode* new_node = new TreeNode(key, c); P.root = P.insertRecursive(P.root, new_node);
            }
        }
    }
    Initialize_vuw(G,Lor,P.root);
};

void GBEES::Initialize_vuw(Grid G,Lorenz3D Lor, TreeNode* r){
    if(r==NULL){ //Iterator over entire BST
        return;
    }
    Initialize_vuw(G,Lor,r->left);
    Initialize_vuw(G,Lor,r->right);  
    
    if((r->cell.active==0)&&(r->key!=-1)){
        std::array<double,DIM> x; 
        for(int i = 0; i < DIM; i++){
            x[i] = G.del*r->cell.state[i];
        }
        
        double v1p = Lor.sigma*(x[1]-(x[0]+G.xh));
        double v2p = -(x[1]+G.xh)-x[0]*x[2];
        double v3p = -Lor.b*(x[2]+G.xh)+x[0]*x[1]-Lor.b*Lor.r; 
        r->cell.vp = {v1p,v2p,v3p};
        r->cell.up = {std::min(v1p,0.0),std::min(v2p,0.0),std::min(v3p,0.0)};
        r->cell.wp = {std::max(v1p,0.0),std::max(v2p,0.0),std::max(v3p,0.0)}; 
        double v1m = Lor.sigma*(x[1]-(x[0]-G.xh));
        double v2m = -(x[1]-G.xh)-x[0]*x[2];
        double v3m = -Lor.b*(x[2]-G.xh)+x[0]*x[1]-Lor.b*Lor.r; 
        r->cell.vm = {v1p,v2p,v3p};
        r->cell.um = {std::min(v1m,0.0),std::min(v2m,0.0),std::min(v3m,0.0)};
        r->cell.wm = {std::max(v1m,0.0),std::max(v2m,0.0),std::max(v3m,0.0)}; 
        r->cell.active = 1;
    }
};

void GBEES::Modify_pointset(Grid G, Lorenz3D Lor){
    double prob_sum = 0; 
    P.root = create_neighbors(G, P.root, prob_sum);
    P.root = delete_neighbors(G, P.root);
    Initialize_vuw(G, Lor, P.root); 
    normalize_prob(P.root, prob_sum); 
    if(!P.isBalanced(P.root)){
        P.root = P.balance(P.root);
    }
};

TreeNode* GBEES::create_neighbors(Grid G, TreeNode* r, double& prob_sum){
    if (r == NULL){
        return r;
    }
    create_neighbors(G, r->left, prob_sum);
    create_neighbors(G, r->right, prob_sum);

    if (r->cell.prob >= G.thresh){
        prob_sum += r->cell.prob; 
        std::array<double,DIM> current_v = r->cell.vp; std::array<int,DIM> current_state = r->cell.state;    

        int x_c = 0; int y_c = 0; int z_c = 0; int x_n[DIM]; int y_n[DIM]; int z_n[DIM];
        
        if(current_v[0] > 0){ //Getting x neighbors
            x_c = 2; x_n[0] = current_state[0]; x_n[1] = current_state[0]+1;
        }else if (current_v[0] == 0){
            x_c = 3; x_n[0] = current_state[0]-1; x_n[1] = current_state[0]; x_n[2] = current_state[0]+1;
        }else{
            x_c = 2; x_n[0] = current_state[0]-1; x_n[1] = current_state[0];
        }

        if(current_v[1] > 0){ //Getting y neighbors
            y_c = 2; y_n[0] = current_state[1]; y_n[1] = current_state[1]+1;
        }else if (current_v[1] == 0){
            y_c = 3; y_n[0] = current_state[1]-1; y_n[1] = current_state[1]; y_n[2] = current_state[1]+1;
        }else{
            y_c = 2; y_n[0] = current_state[1]-1; y_n[1] = current_state[1];
        }

        if(current_v[2] > 0){ //Getting z neighbors
            z_c = 2; z_n[0] = current_state[2]; z_n[1] = current_state[2]+1;
        }else if (current_v[2] == 0){
            z_c = 3; z_n[0] = current_state[2]-1; z_n[1] = current_state[2]; z_n[2] = current_state[2]+1;
        }else{
            z_c = 2; z_n[0] = current_state[2]-1; z_n[1] = current_state[2];
        }

        for (int i = 0; i < x_c; i++){
            for (int j = 0; j < y_c; j++){
                for (int k = 0; k < z_c; k++){
                    std::array<int,DIM> new_state = {x_n[i], y_n[j], z_n[k]}; int new_key = state_conversion(new_state);
                    TreeNode* test_node = P.recursiveSearch(P.root, new_key); 
                    if(test_node == NULL){
                        Cell c = {.prob = 0, .vp = {0}, .up = {0}, .wp = {0}, .vm = {0}, .um = {0}, .wm = {0}, .f = {0}, .state = new_state, .active = 0, .K = 0};
                        TreeNode* new_node = new TreeNode(new_key, c);
                        P.root = P.insertRecursive(P.root, new_node); 
                    }                       
                }
            }
        }       
    }
    return P.root; 
};

TreeNode* GBEES::delete_neighbors(Grid G, TreeNode* r){
    if (r==NULL){
        return r;
    }
    delete_neighbors(G, r->left);
    delete_neighbors(G, r->right);

    if ((r->cell.prob < G.thresh)&&(r->cell.active == 1)){
        bool neighbors = true; std::array<double,DIM> current_v = r->cell.vm; std::array<int,DIM> current_state = r->cell.state;    

        int x_c = 0; int y_c = 0; int z_c = 0; int x_n[DIM]; int y_n[DIM]; int z_n[DIM];
        
        if(current_v[0] < 0){ //Getting x neighbors
            x_c = 2; x_n[0] = current_state[0]; x_n[1] = current_state[0]+1;
        }else if (current_v[0] == 0){
            x_c = 3; x_n[0] = current_state[0]-1; x_n[1] = current_state[0]; x_n[2] = current_state[0]+1;
        }else{
            x_c = 2; x_n[0] = current_state[0]-1; x_n[1] = current_state[0];
        }

        if(current_v[1] < 0){ //Getting y neighbors
            y_c = 2; y_n[0] = current_state[1]; y_n[1] = current_state[1]+1;
        }else if (current_v[1] == 0){
            y_c = 3; y_n[0] = current_state[1]-1; y_n[1] = current_state[1]; y_n[2] = current_state[1]+1;
        }else{
            y_c = 2; y_n[0] = current_state[1]-1; y_n[1] = current_state[1];
        }

        if(current_v[2] < 0){ //Getting z neighbors
            z_c = 2; z_n[0] = current_state[2]; z_n[1] = current_state[2]+1;
        }else if (current_v[2] == 0){
            z_c = 3; z_n[0] = current_state[2]-1; z_n[1] = current_state[2]; z_n[2] = current_state[2]+1;
        }else{
            z_c = 2; z_n[0] = current_state[2]-1; z_n[1] = current_state[2];
        }

        for (int i = 0; i < x_c; i++){
            for (int j = 0; j < y_c; j++){
                for (int k = 0; k < z_c; k++){
                    std::array<int,DIM> new_state = {x_n[i], y_n[j], z_n[k]}; int new_key = state_conversion(new_state);
                    TreeNode* new_node = P.recursiveSearch(P.root, new_key); 
                    if(new_node != NULL){
                        if(new_node->cell.prob >= G.thresh){
                            neighbors = false;
                            break; 
                        }
                    }                  
                }
            }
        }   
        if(neighbors){
            P.root = P.deleteNode(P.root, r->key);
        }
    }
    
    return P.root; 
};

void GBEES::normalize_prob(TreeNode* r, double prob_sum){
    if (r==NULL){
        return;
    }
    normalize_prob(r->left, prob_sum);
    normalize_prob(r->right, prob_sum);

    double current_p = r->cell.prob;

    r->cell.prob = current_p/prob_sum; 
};

void GBEES::RHS_P(Grid G,Lorenz3D Lor){
    initial_f(G, P.root); 
    total_f(G, P.root); 
    calculating_K(G, P.root);
};

void GBEES::initial_f(Grid G, TreeNode* r){
    if (r==NULL){
        return; 
    }
    initial_f(G,r->left);
    initial_f(G,r->right);

    int l_key = r->key;
    if(l_key != -1){
        std::array<int, DIM> l_state = r->cell.state; r->cell.f = {0.0}; r->cell.K = 0.0; 
        for(int q = 0; q < DIM; q++){
            //Initializing k nodes
            std::array<int,DIM> k_state = l_state; k_state[q] = k_state[q]+1; int k_key = state_conversion(k_state);
            TreeNode* k_node = P.recursiveSearch(P.root, k_key); if(k_node == NULL) k_node = dead; r->cell.k_nodes[q] = k_node;      
            r->cell.f[q] = r->cell.wp[q] * r->cell.prob + r->cell.up[q] * k_node->cell.prob;
        }
    }
};

void GBEES::total_f(Grid G, TreeNode* r){
    if (r==NULL){
        return;
    }
    total_f(G,r->left);
    total_f(G,r->right);

    int l_key = r->key; 
    if(l_key != -1){ 

        //Initializing i, j, and p nodes
        TreeNode* j_nodes[DIM][DIM]; 
        TreeNode* p_nodes[DIM][DIM];

        for(int q = 0; q < DIM; q++){
            std::array<int,DIM> i_state = r->cell.state; i_state[q] = i_state[q]-1; int i_key = state_conversion(i_state);
            TreeNode* i_node = P.recursiveSearch(P.root, i_key); if(i_node == NULL) i_node = dead; r->cell.i_nodes[q] = i_node;
            if ((r->cell.prob >= G.thresh)||(i_node->cell.prob >= G.thresh)){ 
                for(int e = 0; e < DIM; e++){
                    if (e!=q){
                        j_nodes[e][q] = i_node;
                        if(e>q){
                            if(i_node != dead){
                                std::array<int,DIM> p_state = i_state; p_state[e] = p_state[e]-1; int p_key = state_conversion(p_state);
                                TreeNode* p_node = P.recursiveSearch(P.root, p_key); if(p_node == NULL) p_node = dead; p_nodes[q][e] = p_node;  
                            }else{
                                TreeNode* p_node = dead; p_nodes[q][e] = p_node;  
                            }
                        }
                    }
                }
            }else{
                for(int e = 0; e < DIM; e++){
                    if (e!=q){
                        j_nodes[e][q] = dead;
                        if(e>q){
                            TreeNode* p_node = dead; p_nodes[q][e] = p_node; 
                        }
                    }
                }                            
            }
        }

        //Make p diagonally symmetric
        for(int q = 0; q < DIM; q++){
            for(int e = 0; e < DIM; e++){
                if(e<q){
                    p_nodes[q][e] = p_nodes[e][q];
                }
            }
        }

        for(int q = 0; q < DIM; q++){
            TreeNode* i_node = r->cell.i_nodes[q]; 
            if ((r->cell.prob >= G.thresh)||(i_node->cell.prob >= G.thresh)){ 
                double F = G.dt*(r->cell.prob-i_node->cell.prob)/(2*G.del); 
                for(int e = 0; e < DIM; e++){
                    if (e!=q){
                        TreeNode* j_node = j_nodes[q][e]; TreeNode* p_node = p_nodes[q][e]; 

                        r->cell.f[e] -= r->cell.wp[e] * i_node->cell.wp[q] * F;
                        i_node->cell.f[e] -= i_node->cell.wp[e] * i_node->cell.up[q] * F;
                        j_node->cell.f[e] -= j_node->cell.up[e] * i_node->cell.wp[q] * F;
                        p_node->cell.f[e] -= p_node->cell.up[e] * i_node->cell.up[q] * F; 
                    }                       
                }
                
                double th,t; 
                if (r->cell.i_nodes[q]->cell.vp[q]>0){
                    std::array<int,DIM> i_i_state = i_node->cell.state; i_i_state[q] = i_i_state[q]-1; int i_i_key = state_conversion(i_i_state);
                    TreeNode* i_i_node = P.recursiveSearch(P.root, i_i_key); if(i_i_node == NULL) i_i_node = dead; 
                    th = (i_node->cell.prob-i_i_node->cell.prob)/(r->cell.prob-i_node->cell.prob);
                }else{
                    th = (r->cell.k_nodes[q]->cell.prob-r->cell.prob)/(r->cell.prob-i_node->cell.prob);
                }

                if(i_node->cell.vp[q]>=0){
                    t = i_node->cell.vp[q];
                }else{
                    t = -i_node->cell.vp[q];
                } 
                
                i_node->cell.f[q] += t*(G.del/G.dt - t)*F*MC(th); 
            }
        }
    }
};

void GBEES::calculating_K(Grid G, TreeNode* r){
    if (r == NULL){
        return;
    }
    calculating_K(G,r->left);
    calculating_K(G,r->right);

    int l_key = r->key;
    if(l_key != -1){
        std::array<int,DIM> l_state = r->cell.state; 
        for(int q = 0; q < DIM; q++){
            r->cell.K -= (r->cell.f[q]-r->cell.i_nodes[q]->cell.f[q])/G.del;  
        }
    }
};

void GBEES::update_prob(Grid G, TreeNode* r){
    if (r == NULL){
        return; 
    }

    update_prob(G,r->left);
    update_prob(G,r->right);
    
    if(r->key != -1){
        r->cell.prob += G.dt*r->cell.K; 
    }
};

void GBEES::Record_Data(std::string file_name, Grid G, TreeNode* r){
	std::ofstream myfile; myfile.open(file_name);
    P.writeFile(myfile, G, P.root);
    myfile.close(); 
};

#endif // GBEES_H