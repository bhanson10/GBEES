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
    void normalize_prob(TreeNode* r, double prob_sum);
    void RHS_P(Grid G,Lorenz3D Lor);
    void initial_f(Grid G, TreeNode* r); 
    void total_f(Grid G, TreeNode* r); 
    void calculating_K(Grid G, TreeNode* r);
    void Record_Data(std::string file_name, Grid G, TreeNode* r);
};
/*==============================================================================
Non-member Function DEFINITIONS
==============================================================================*/
int CantorPair(int state[], int n){
    int key; 
    if(n>2){
        int last = state[n-1]; int new_state[n-1];
        for (int i = 0; i < n - 1; i++){
            new_state[i] = state[i];
        }
        int x = CantorPair(new_state, n-1); int y = last;
        key = (0.5)*(x+y)*(x+y+1)+y;
    }else{
        int x = state[0]; int y = state[1];
        key = (0.5)*(x+y)*(x+y+1)+y;
    }
    return key; 
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
    if(DIM == 1){
        int key = shift_state[0];
        return key;
    }else{
        int key = CantorPair(shift_state, DIM);
        return key;
    }
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
    for (int i = round((G.start[0]-2*G.std[0])/G.del[0]); i <= round((G.start[0]+2*G.std[0])/G.del[0]); i++){
        for (int j = round((G.start[1]-2*G.std[1])/G.del[1]); j <= round((G.start[1]+2*G.std[1])/G.del[1]); j++){
            for (int k = round((G.start[2]-2*G.std[2])/G.del[2]); k <= round((G.start[2]+2*G.std[2])/G.del[2]); k++){
                current_state = {i,j,k}; key = state_conversion(current_state);

                double x = 0; 
                for(int q = 0; q < DIM; q++){
                    x += pow((current_state[q]*G.del[q]-G.start[q]),2)/pow(G.std[q],2); 
                }
                
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
            x[i] = G.del[i]*r->cell.state[i];
        }
        
        double v1p = Lor.sigma*(x[1]-(x[0]+G.xh[0]));
        double v2p = -(x[1]+G.xh[1])-x[0]*x[2];
        double v3p = -Lor.b*(x[2]+G.xh[2])+x[0]*x[1]-Lor.b*Lor.r; 
        r->cell.vp = {v1p,v2p,v3p};
        r->cell.up = {std::min(v1p,0.0),std::min(v2p,0.0),std::min(v3p,0.0)};
        r->cell.wp = {std::max(v1p,0.0),std::max(v2p,0.0),std::max(v3p,0.0)}; 
        double v1m = Lor.sigma*(x[1]-(x[0]-G.xh[0]));
        double v2m = -(x[1]-G.xh[1])-x[0]*x[2];
        double v3m = -Lor.b*(x[2]-G.xh[2])+x[0]*x[1]-Lor.b*Lor.r; 
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
        
        std::array<int,DIM> num_n; std::array<std::array<int,DIM>,DIM> which_n;  

        for(int c = 0; c < DIM; c++){
            if(current_v[c] > 0){
                num_n[c] = 2; 
                which_n[c][0] = current_state[c]; which_n[c][1] = current_state[c]+1;
            }else if(current_v[c]==0){
                num_n[c] = 3; 
                which_n[c][0] = current_state[c]-1; which_n[c][1] = current_state[c]; which_n[c][2] = current_state[c]+1;
            }else{
                num_n[c] = 2;
                which_n[c][0] = current_state[c]-1; which_n[c][1] = current_state[c];
            }
        }

        for (int i = 0; i < num_n[0]; i++){
            for (int j = 0; j < num_n[1]; j++){
                for (int k = 0; k < num_n[2]; k++){
                    std::array<int,DIM> new_state = {which_n[0][i], which_n[1][j], which_n[2][k]}; int new_key = state_conversion(new_state);
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

        std::array<int,DIM> num_n; std::array<std::array<int,DIM>,DIM> which_n;  

        for(int c = 0; c < DIM; c++){
            if(current_v[c] < 0){
                num_n[c] = 2; 
                which_n[c][0] = current_state[c]; which_n[c][1] = current_state[c]+1;
            }else if(current_v[c]==0){
                num_n[c] = 3; 
                which_n[c][0] = current_state[c]-1; which_n[c][1] = current_state[c]; which_n[c][2] = current_state[c]+1;
            }else{
                num_n[c] = 2;
                which_n[c][0] = current_state[c]-1; which_n[c][1] = current_state[c];
            }
        }

        for (int i = 0; i < num_n[0]; i++){
            for (int j = 0; j < num_n[1]; j++){
                for (int k = 0; k < num_n[2]; k++){
                    std::array<int,DIM> new_state = {which_n[0][i], which_n[1][j], which_n[2][k]}; int new_key = state_conversion(new_state);
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
            //Initializing i, k nodes
            std::array<int,DIM> i_state = l_state; i_state[q] = i_state[q]-1; int i_key = state_conversion(i_state);
            TreeNode* i_node = P.recursiveSearch(P.root, i_key); if(i_node == NULL) i_node = dead; r->cell.i_nodes[q] = i_node;      
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
        for(int q = 0; q < DIM; q++){
            TreeNode* i_node = r->cell.i_nodes[q]; 
            if ((r->cell.prob >= G.thresh)||(i_node->cell.prob >= G.thresh)){ 
                double F = G.dt*(r->cell.prob-i_node->cell.prob)/(2*G.del[q]); 
                for(int e = 0; e < DIM; e++){
                    if (e!=q){
                        TreeNode* j_node = r->cell.i_nodes[e]; TreeNode* p_node; 
                        if (i_node != dead){
                            p_node = i_node->cell.i_nodes[e]; 
                        }else{
                            p_node = dead; 
                        }
                        
                        r->cell.f[e] -= r->cell.wp[e] * i_node->cell.wp[q] * F;
                        i_node->cell.f[e] -= i_node->cell.wp[e] * i_node->cell.up[q] * F;
                        j_node->cell.f[e] -= j_node->cell.up[e] * i_node->cell.wp[q] * F;
                        p_node->cell.f[e] -= p_node->cell.up[e] * i_node->cell.up[q] * F; 
                    }                       
                }
                
                double th,t; 
                if (i_node->cell.vp[q]>0){
                    TreeNode* i_i_node = i_node->cell.i_nodes[q];
                    th = (i_node->cell.prob-i_i_node->cell.prob)/(r->cell.prob-i_node->cell.prob);
                    t = i_node->cell.vp[q];
                }else{
                    th = (r->cell.k_nodes[q]->cell.prob-r->cell.prob)/(r->cell.prob-i_node->cell.prob);
                    t = -i_node->cell.vp[q];
                }
                
                i_node->cell.f[q] += t*(G.del[q]/G.dt - t)*F*MC(th); 
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
        for(int q = 0; q < DIM; q++){
            r->cell.K -= (r->cell.f[q]-r->cell.i_nodes[q]->cell.f[q])/G.del[q];  
        }
        r->cell.prob += G.dt*r->cell.K; 
    }
};

void GBEES::Record_Data(std::string file_name, Grid G, TreeNode* r){
	std::ofstream myfile; myfile.open(file_name);
    P.writeFile(myfile, G, P.root);
    myfile.close(); 
};

#endif // GBEES_H