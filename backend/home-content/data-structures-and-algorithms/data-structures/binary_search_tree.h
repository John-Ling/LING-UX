#ifndef BINARY_SEARCH_TREE_H
#define BINARY_SEARCH_TREE_H

#define BAR_WIDTH 4

typedef struct BSTNode_t
{
    int value;
    struct BSTNode_t* left;
    struct BSTNode_t* right;
} BSTNode;

typedef struct BST_t
{
    int itemCount;
    BSTNode* root;
} BST;

/* Tree lifecycle */
BST* create_tree(int arr[], const int n);
int  free_tree(BSTNode* root);

/* Core operations */
int insert(BST* bst, const int value);
int delete(BST* bst, const int value);
int search_tree(BSTNode* root, const int value);

/* Plain display */
int  print_tree(BST* bst);
void visualize_tree(BST* bst);

/* Animated display
 *   animate_inorder  -- steps through every node in sorted order, highlighting
 *                       each one in reverse video as it is visited.
 *   animate_search   -- walks the search path highlighting each comparison;
 *                       shows "Found" or "Not found" at the end.
 *   animate_delete   -- animated search to find the target, highlights the
 *                       inorder successor when the node has two children,
 *                       then performs the deletion and redraws the result.
 */
void animate_inorder(BST* bst);
void animate_search(BST* bst, int value);
void animate_delete(BST* bst, int value);

/* NOTE: insert_node, delete_node, find_min_node, tree_to_array are private   */
/*       (static) helpers in the .c file -- they do not belong in the header. */

#endif /* BINARY_SEARCH_TREE_H */