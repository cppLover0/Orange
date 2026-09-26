#pragma once

#include <cstdint>
#include <generic/lock/spinlock.hpp>

// i used wikipedia as ref

namespace utils {

    enum class rbtree_color {
        whoisthis = 0,
        black = 1,
        red = 2
    };

    struct rbtree_node {

        rbtree_node* parent = nullptr;

        rbtree_node* right = nullptr;
        rbtree_node* left = nullptr;

        rbtree_color color = rbtree_color::whoisthis;
        std::uintptr_t x = 0; 

        void* value = nullptr;

        rbtree_node* grandparent() {
            if(this->parent == nullptr) 
                return nullptr;
            else 
                return this->parent->parent;
        }

        rbtree_node* uncle() {
            auto grandpa = this->grandparent();
            if(grandpa == nullptr)
                return nullptr;

            if(this->parent == grandpa->left)
                return grandpa->right;
            else 
                return grandpa->left;
        }

        rbtree_node* bro() {
            if(this == this->parent->left)
                return this->parent->right;
            else 
                return this->parent->left;
        }

        void rotate_left(rbtree_node*& root) {
            auto right = this->right;
            if (right == nullptr) return;

            right->parent = this->parent;
            if (this->parent != nullptr) {
                if (this->parent->left == this)
                    this->parent->left = right;
                else
                    this->parent->right = right;
            } else {
                root = right;
            }

            this->right = right->left;
            if (right->left != nullptr)
                right->left->parent = this;

            this->parent = right;
            right->left = this;
        }

        void rotate_right(rbtree_node*& root) {
            auto left = this->left;
            if (left == nullptr) return;

            left->parent = this->parent;
            if (this->parent != nullptr) {
                if (this->parent->left == this)
                    this->parent->left = left;
                else
                    this->parent->right = left;
            } else {
                root = left;
            }

            this->left = left->right;
            if (left->right != nullptr)
                left->right->parent = this;

            this->parent = left;
            left->right = this;
        }

    };

    template <typename T>
    class rbtree {
    private:


        rbtree_node* root = nullptr;
        locks::spinlock lock;
        
        rbtree_node* find(const std::uintptr_t& key) {
            auto current = root;

            while(current) {
                if(current->x == key)
                    return current;
                else if(key < current->x) 
                    current = current->left;
                else if(key > current->x)
                    current = current->right;
            }

            return nullptr;
        }

        rbtree_node* insert_final(const T& value, rbtree_node* node) {
            if(node->value != nullptr)
                delete static_cast<T*>(node->value);

            T* new_value = new T;
            *new_value = value;
            node->value = static_cast<void*>(new_value);
            return node;
        }

        rbtree_node* insert_recursive(const std::uintptr_t& key, const T& value, rbtree_node* parent, rbtree_node*& node) {
            if(node == nullptr) {
                node = new rbtree_node;
                node->color = rbtree_color::red;
                node->parent = parent;
                node->x = key;
            } 

            if(node->x == key) {
                return insert_final(value, node);
            } else if(key > node->x) {
                return insert_recursive(key, value, node, node->right);
            } else if(key < node->x) {
                return insert_recursive(key, value, node, node->left);
            }

            assert(0, "shit.");
            return nullptr;
        }

        void insert(const std::uintptr_t& key, const T& value) {
            this->lock.lock();
            auto node = insert_recursive(key, value, nullptr, this->root);
            fix_insert(node);
            this->lock.unlock();
        }

        void remove_recursive(const std::uintptr_t& key, rbtree_node*& node) {
            if (node == nullptr)
                return;

            auto remove_node = [](rbtree_node* node) {
                if (node->value != nullptr)
                    delete static_cast<T*>(node->value);
                delete node;
            };

            if (key == node->x) {
                rbtree_color deleted_color = node->color;

                if (node->right == nullptr && node->left == nullptr) {
                    if (deleted_color == rbtree_color::black) {
                        fix_remove(node);
                    }
                    remove_node(node);
                    node = nullptr;

                } else if (node->right != nullptr && node->left == nullptr) {
                    auto old_node = node;
                    node = node->right;
                    node->parent = old_node->parent;

                    if (deleted_color == rbtree_color::black) {
                        fix_remove(node);
                    }
                    remove_node(old_node);

                } else if (node->left != nullptr && node->right == nullptr) {
                    auto old_node = node;
                    node = node->left;
                    node->parent = old_node->parent;

                    if (deleted_color == rbtree_color::black) {
                        fix_remove(node);
                    }
                    remove_node(old_node);

                } else {
                    rbtree_node** successor_ref = &node->right;
                    while ((*successor_ref)->left != nullptr) {
                        successor_ref = &(*successor_ref)->left;
                    }

                    rbtree_node* successor = *successor_ref;
                    rbtree_color successor_color = successor->color;
                    rbtree_node* successor_parent = successor->parent;
                    rbtree_node* successor_right = successor->right;

                    if (successor->parent != node) {
                        *successor_ref = successor->right;
                        if (successor->right != nullptr) {
                            successor->right->parent = successor_parent;
                        }
                    }

                    successor->color = node->color;
                    successor->left = node->left;
                    if (successor->left != nullptr) successor->left->parent = successor;

                    if (successor->parent != node) {
                        successor->right = node->right;
                        if (successor->right != nullptr) successor->right->parent = successor;
                    }

                    successor->parent = node->parent;

                    auto old_node = node;
                    node = successor;

                    if (successor_color == rbtree_color::black) {
                        if (successor_right != nullptr) {
                            fix_remove(successor_right);
                        } else if (successor->parent != nullptr) {
                            if (successor->parent->left == successor) {
                                fix_remove(successor);
                            } else if (successor->parent->right == successor) {
                                fix_remove(successor);
                            }
                        }
                    }

                    remove_node(old_node);
                }
            } else if (key > node->x) {
                remove_recursive(key, node->right);
            } else if (key < node->x) {
                remove_recursive(key, node->left);
            }
        }

        void fix_insert(rbtree_node* node) {
            if(node->parent == nullptr) {
                node->color = rbtree_color::black;
                return;
            }

            if(node->parent->color == rbtree_color::black)
                return;
            
            auto uncle = node->uncle();
            auto grandparent = node->grandparent();

            if(uncle != nullptr && uncle->color == rbtree_color::red) {
                node->parent->color = rbtree_color::black;
                uncle->color = rbtree_color::black;
                grandparent->color = rbtree_color::red;
                fix_insert(grandparent);
                return;
            }

            grandparent = node->grandparent();
            if(node == node->parent->right && node->parent == grandparent->left) {
                node->parent->rotate_left(this->root);
                node = node->left;
            } else if(node == node->parent->left && node->parent == grandparent->right) {
                node->parent->rotate_right(this->root);
                node = node->right;
            }

            grandparent = node->grandparent();
            
            node->parent->color = rbtree_color::black;
            grandparent->color = rbtree_color::red;
            if(node == node->parent->left && node->parent == grandparent->left) {
                grandparent->rotate_right(this->root);
            } else {
                grandparent->rotate_left(this->root);
            }
        }

        void fix_remove(rbtree_node* node) {
            while (node != this->root && node->color == rbtree_color::black) {
                if (node->parent == nullptr)
                    return;

                auto bro = node->bro();
                if (bro == nullptr)
                    return;

                if (node == node->parent->left) {
                    if (bro->color == rbtree_color::red) {
                        bro->color = rbtree_color::black;
                        node->parent->color = rbtree_color::red;
                        node->parent->rotate_left(this->root);
                        bro = node->parent->right;
                        if (bro == nullptr) return;
                    }

                    if ((bro->left == nullptr || bro->left->color == rbtree_color::black) && (bro->right == nullptr || bro->right->color == rbtree_color::black)) {
                        bro->color = rbtree_color::red;
                        node = node->parent;
                    } else {
                        if (bro->right == nullptr || bro->right->color == rbtree_color::black) {
                            if (bro->left != nullptr)
                                bro->left->color = rbtree_color::black;
                            bro->color = rbtree_color::red;
                            bro->rotate_right(this->root);
                            bro = node->parent->right;
                            if (bro == nullptr) return;
                        }

                        bro->color = node->parent->color;
                        node->parent->color = rbtree_color::black;
                        if (bro->right != nullptr)
                            bro->right->color = rbtree_color::black;
                        node->parent->rotate_left(this->root);
                        node = this->root;
                        break;
                    }
                } else {
                    if (bro->color == rbtree_color::red) {
                        bro->color = rbtree_color::black;
                        node->parent->color = rbtree_color::red;
                        node->parent->rotate_right(this->root);
                        bro = node->parent->left;
                        if (bro == nullptr) return;
                    }

                    if ((bro->left == nullptr || bro->left->color == rbtree_color::black) && (bro->right == nullptr || bro->right->color == rbtree_color::black)) {
                        bro->color = rbtree_color::red;
                        node = node->parent;
                    } else {
                        if (bro->left == nullptr || bro->left->color == rbtree_color::black) {
                            if (bro->right != nullptr)
                                bro->right->color = rbtree_color::black;
                            bro->color = rbtree_color::red;
                            bro->rotate_left(this->root);
                            bro = node->parent->left;
                            if (bro == nullptr) return;
                        }

                        bro->color = node->parent->color;
                        node->parent->color = rbtree_color::black;
                        if (bro->left != nullptr)
                            bro->left->color = rbtree_color::black;
                        node->parent->rotate_right(this->root);
                        node = this->root;
                        break;
                    }
                }
            }
            if (node != nullptr)
                node->color = rbtree_color::black;
        }

        void clear(rbtree_node* node) {
            if(node == nullptr) 
                return;

            clear(node->right);
            clear(node->left);

            if(node->value != nullptr)
                delete static_cast<T*>(node->value);

            delete node;
        }

    public:

        T* get(const std::uintptr_t& key) {
            this->lock.lock();

            auto node = this->find(key);
            if(node == nullptr) {
                this->lock.unlock();
                return nullptr;
            }

            T* value = static_cast<T*>(node->value);
            this->lock.unlock();
            return value;
        }

        ~rbtree() {
            this->lock.lock();
            this->clear(this->root);
            this->lock.unlock();
        }

        T& operator[](const std::uintptr_t& key) {
            if(this->get(key) == nullptr) {
                T empty_type = {};
                this->insert(key, empty_type);
                assert(this->get(key) != nullptr, "i shited myself");
            }

            return *static_cast<T*>(this->get(key));
        }

        void remove(const std::uintptr_t& key) {
            this->lock.lock();
            this->remove_recursive(key, this->root);
            this->lock.unlock();
        }

        // start from the most left elements
        inline void start(rbtree_node*& out) {
            this->lock.lock();
            out = this->root;
            while(out->left != nullptr)
                out = out->left;
            this->lock.unlock();
        }

        // iterate
        inline T* next(rbtree_node*& p) {
            if (p == nullptr)
                return nullptr;

            this->lock.lock();

            auto cur = p;
            if (cur->right != nullptr) {
                cur = cur->right;
                while (cur->left != nullptr)
                    cur = cur->left;
            } else {

                rbtree_node* parent = cur->parent;
                while (parent != nullptr && cur == parent->right) {
                    cur = parent;
                    parent = parent->parent;
                }
                cur = parent; 
            }

            p = cur;

            if (cur == nullptr) {
                this->lock.unlock();
                return nullptr;
            }

            T* value = static_cast<T*>(cur->value);
            this->lock.unlock();
            return value;
        }

    };
};