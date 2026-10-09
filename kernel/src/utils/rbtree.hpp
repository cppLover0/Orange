#pragma once

#include <cstdint>
#include <generic/lock/spinlock.hpp>

// i used wikipedia as ref

// i made my implementation to be able to work like linked list too (start, next functions)

namespace utils {

    enum class rbtree_color {
        whoisthis = 0,
        black = 1,
        red = 2
    };

    template <typename key_type>
    struct rbtree_node {

        rbtree_node* parent = nullptr;

        rbtree_node* right = nullptr;
        rbtree_node* left = nullptr;

        rbtree_color color = rbtree_color::whoisthis;
        key_type x = 0; 

        std::atomic<std::size_t> size = 0;
        std::atomic<std::size_t> ref = 1;

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

    template <typename T, typename key_type>
    class rbtree {
    private:


        rbtree_node<key_type>* root = nullptr;
        locks::spinlock lock; 
        
        rbtree_node<key_type>* find(const key_type& key) {
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

        rbtree_node<key_type>* insert_final(const T& value, rbtree_node<key_type>* node) {
            if(node->value != nullptr)
                delete static_cast<T*>(node->value);

            T* new_value = new T;
            *new_value = value;
            node->value = static_cast<void*>(new_value);
            return node;
        }

        rbtree_node<key_type>* insert_recursive(const key_type& key, const T& value, rbtree_node<key_type>* parent, rbtree_node<key_type>*& node) {
            if(node == nullptr) {
                node = new rbtree_node<key_type>;
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

        rbtree_node<key_type>& insert(const key_type& key, const T& value, bool should_lock = true) {
            if(should_lock == true)
                this->lock.lock();
            auto node = insert_recursive(key, value, nullptr, this->root);
            fix_insert(node);
            if(should_lock == true)
                this->lock.unlock();
            return *node;
        }

        void remove_recursive(const key_type& key, rbtree_node<key_type>*& node) {
            if (node == nullptr)
                return;

            auto remove_node = [](rbtree_node<key_type>* node) {
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
                    rbtree_node<key_type>** successor_ref = &node->right;
                    while ((*successor_ref)->left != nullptr) {
                        successor_ref = &(*successor_ref)->left;
                    }

                    rbtree_node<key_type>* successor = *successor_ref;
                    rbtree_color successor_color = successor->color;
                    rbtree_node<key_type>* successor_parent = successor->parent;
                    rbtree_node<key_type>* successor_right = successor->right;

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

        void fix_insert(rbtree_node<key_type>* node) {
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

        void fix_remove(rbtree_node<key_type>* node) {
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

        void clear(rbtree_node<key_type>* node) {
            if(node == nullptr) 
                return;

            clear(node->right);
            clear(node->left);

            if(node->value != nullptr)
                delete static_cast<T*>(node->value);

            delete node;
        }

        rbtree_node<key_type>* search_for_the_most_left(rbtree_node<key_type>* node) {
            auto current = node;
            while(current) {
                if(current->left == nullptr)
                    return current;

                current = current->left;
            }

            return nullptr;
        }

        rbtree_node<key_type>* find_gap_child(rbtree_node<key_type>* node, const std::size_t& size) {

            if(node->right != nullptr) {
                auto leftest = this->search_for_the_most_left(node);
                if(leftest->x - node->x >= size)
                    return node;

                auto result = this->find_gap_child(node->right, size);

                if(result != nullptr)
                    return result;
            }

            return nullptr;
        }

        rbtree_node<key_type>* find_gap_left(rbtree_node<key_type>* node, const std::size_t& size) {

            if(node->right != nullptr) {
                auto leftest = this->search_for_the_most_left(node->right);
                if(leftest->x - node->x >= size)
                    return node;

                auto result = this->find_gap_child(node->right, size);

                if(result != nullptr)
                    return result;
            } else {
                if(node->parent != nullptr) {
                    if(node->parent->x - node->x >= size)
                        return node;
                }
            }

            if(node->parent != nullptr)
                return find_gap_left(node->parent, size);
            
            return nullptr;
        }

    public:

        T* get(const key_type& key) {
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

        T& operator[](const key_type& key) {
            if(this->get(key) == nullptr) {
                T empty_type = {};
                this->insert(key, empty_type);
                assert(this->get(key) != nullptr, "i shited myself");
            }

            return *static_cast<T*>(this->get(key));
        }

        T* insert_object(const key_type& key, const T& value_obj) {
            return static_cast<T*>(this->insert(key, value_obj).value);
        }

        T* insert_object_add_ref(const key_type& key, const T& value_obj) {
            this->lock.lock();

            if(auto node = this->find(key); node != nullptr) {
                node->ref++;
                this->lock.unlock();
                return static_cast<T*>(node->value);
            }

            auto value = static_cast<T*>(this->insert(key, value_obj, false).value);

            this->lock.unlock();
            return value;
        }

        T* insert_size(const key_type& key, const std::size_t& size, const T& value_obj) {
            rbtree_node<key_type>& node = this->insert(key, value_obj);
            node.size = size;
            return static_cast<T*>(node.value);
        }

        T* find_size(const std::size_t& size, rbtree_node<key_type>* node = nullptr) {

            if(this->root == nullptr)
                return nullptr;

            if(node == nullptr) {
                this->lock.lock();
                node = this->root;
            }

            T* res = nullptr;

            if(node->size >= size) {
                res = static_cast<T*>(node->value);
                goto done;
            }

            if(node->left != nullptr) {
                if(node->left->size >= size) {
                    res = static_cast<T*>(node->left->value);
                    goto done;
                }

                res = find_size(size, node->left);
                if(res != nullptr)
                    goto done;
            }

            if(node->right != nullptr) {
                if(node->right->size >= size) {
                    res = static_cast<T*>(node->right->value);
                    goto done;
                }

                res = find_size(size, node->right);
                if(res != nullptr)
                    goto done;
            }

done:
            if(node == this->root)
                this->lock.unlock();

            return res;
        }

        T* find_gap(const std::size_t& size) {
            this->lock.lock();

            if(this->root == nullptr) {
                this->lock.unlock();
                return nullptr;
            }

            auto leftest = this->search_for_the_most_left(this->root);
            if(leftest == nullptr) {
                this->lock.unlock();
                return nullptr;
            }

            auto node = this->find_gap_left(leftest, size);

            if(node == nullptr) {
                this->lock.unlock();
                return nullptr;
            }


            T* value = static_cast<T*>(node->value);
            this->lock.unlock();

            return value;
        }

        void remove(const key_type& key) {
            this->lock.lock();

            if(auto node = this->find(key); node != nullptr) {
                if(--node->ref != 0)
                    return;
            }

            this->remove_recursive(key, this->root);
            this->lock.unlock();
        }

        // start from the most left elements
        inline T* start(rbtree_node<key_type>*& out) {
            this->lock.lock();
            out = this->root;
            while(out->left != nullptr)
                out = out->left;
            this->lock.unlock();
            return static_cast<T*>(out->value);
        }

        // end
        inline T* end(rbtree_node<key_type>*& out) {
            this->lock.lock();
            out = this->root;
            while(out->right != nullptr)
                out = out->right;
            this->lock.unlock();
            return static_cast<T*>(out->value);
        }

        // iterate
        inline T* next(rbtree_node<key_type>*& p) {
            if(p == nullptr)
                return nullptr;

            this->lock.lock();

            auto current = p;
            if(current->right != nullptr) {
                current = current->right;
                while(current->left != nullptr)
                    current = current->left;
            } else {

                rbtree_node<key_type>* parent = current->parent;
                while(parent != nullptr && current == parent->right) {
                    current = parent;
                    parent = parent->parent;
                }
                current = parent; 
            }

            p = current;

            if(current == nullptr) {
                this->lock.unlock();
                return nullptr;
            }

            T* value = static_cast<T*>(current->value);
            this->lock.unlock();
            return value;
        }

    };
};