#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <iomanip>
#include <ctime>

//Códigos para colores
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

namespace fs = std::filesystem;

// Definición de la estructura INode
struct INode {
    std::string name;
    size_t size;
    std::string permissions;
    unsigned int id;
    bool isDirectory;
    std::time_t creationTime;
    std::vector<std::shared_ptr<INode>> children;
    std::shared_ptr<INode> parent;

    INode(std::string name, bool isDirectory, std::shared_ptr<INode> parent)
        : name(name), size(0), permissions("rwxr--r--"), id(0), isDirectory(isDirectory), parent(parent) {
            creationTime = std::time(nullptr);
        }
};

// Definición de la clase BTree
class BTree {
public:
    BTree(int t);
    void traverse();
    std::shared_ptr<INode> search(unsigned int k);
    void insert(std::shared_ptr<INode> k);

private:
    struct BTreeNode {
        std::vector<std::shared_ptr<INode>> keys;
        std::vector<std::shared_ptr<BTreeNode>> children;
        bool leaf;
        int t;

        BTreeNode(int t, bool leaf);
        void traverse();
        std::shared_ptr<INode> search(unsigned int k);
        void insertNonFull(std::shared_ptr<INode> k);
        void splitChild(int i, std::shared_ptr<BTreeNode> y);
    };

    std::shared_ptr<BTreeNode> root;
    int t;
};

// Implementación de la clase BTree
BTree::BTree(int t) : t(t), root(nullptr) {}

void BTree::traverse() {
    if (root != nullptr) root->traverse();
}

std::shared_ptr<INode> BTree::search(unsigned int k) {
    return (root == nullptr) ? nullptr : root->search(k);
}

void BTree::insert(std::shared_ptr<INode> k) {
    if (root == nullptr) {
        root = std::make_shared<BTreeNode>(t, true);
        root->keys.push_back(k);
    } else {
        if (root->keys.size() == 2 * t - 1) {
            auto s = std::make_shared<BTreeNode>(t, false);
            s->children.push_back(root);
            s->splitChild(0, root);
            int i = 0;
            if (s->keys[0]->id < k->id) i++;
            s->children[i]->insertNonFull(k);
            root = s;
        } else {
            root->insertNonFull(k);
        }
    }
}

BTree::BTreeNode::BTreeNode(int t, bool leaf) : t(t), leaf(leaf) {}

void BTree::BTreeNode::traverse() {
    int i;
    for (i = 0; i < keys.size(); i++) {
        if (!leaf) children[i]->traverse();
        std::cout << " " << keys[i]->id;
    }
    if (!leaf) children[i]->traverse();
}

std::shared_ptr<INode> BTree::BTreeNode::search(unsigned int k) {
    int i = 0;
    while (i < keys.size() && k > keys[i]->id) i++;
    if (i < keys.size() && keys[i]->id == k) return keys[i];
    return (leaf) ? nullptr : children[i]->search(k);
}

void BTree::BTreeNode::insertNonFull(std::shared_ptr<INode> k) {
    int i = keys.size() - 1;
    if (leaf) {
        keys.push_back(k);
        while (i >= 0 && keys[i]->id > k->id) {
            keys[i + 1] = keys[i];
            i--;
        }
        keys[i + 1] = k;
    } else {
        while (i >= 0 && keys[i]->id > k->id) i--;
        if (children[i + 1]->keys.size() == 2 * t - 1) {
            splitChild(i + 1, children[i + 1]);
            if (keys[i + 1]->id < k->id) i++;
        }
        children[i + 1]->insertNonFull(k);
    }
}

void BTree::BTreeNode::splitChild(int i, std::shared_ptr<BTreeNode> y) {
    auto z = std::make_shared<BTreeNode>(y->t, y->leaf);
    z->keys.insert(z->keys.end(), y->keys.begin() + t, y->keys.end());
    y->keys.resize(t - 1);

    if (!y->leaf) {
        z->children.insert(z->children.end(), y->children.begin() + t, y->children.end());
        y->children.resize(t);
    }

    children.insert(children.begin() + i + 1, z);
    keys.insert(keys.begin() + i, y->keys[t - 1]);
    y->keys.pop_back();
}

// Definición de la clase FileSystem
class FileSystem {
private:
    BTree tree;
    std::shared_ptr<INode> root;
    std::shared_ptr<INode> currentDirectory;
    unsigned int nextId;
    fs::path rootPath;

public:
    FileSystem();
    void createFile(const std::string& name);
    void createDirectory(const std::string& name);
    void renameNode(const std::string& oldName, const std::string& newName);
    void deleteNode(const std::string& name);
    void changePermissions(const std::string& name, const std::string& permissions);
    void listFiles();
    void listFilesRecursively(std::shared_ptr<INode> node, int depth);
    void searchFileByName(const std::string& name);
    void showCurrentDirectory();
    void saveToFile(const std::string& filename);
    void loadFromFile(const std::string& filename);
    std::shared_ptr<INode> getCurrentDirectory();
    void changeDirectory(const std::string& name);
    void changeToParentDirectory();
    void mapFileSystem(const fs::path& path, std::shared_ptr<INode> parentNode);
    void displayHelp(); // Función para mostrar los comandos disponibles
};

// Nuevo método para mapear el sistema de archivos
void FileSystem::mapFileSystem(const fs::path& path, std::shared_ptr<INode> parentNode) {
    for (const auto& entry : fs::directory_iterator(path)) {
        auto node = std::make_shared<INode>(entry.path().filename().string(), entry.is_directory(), parentNode);
        node->id = nextId++;
        node->size = entry.is_directory() ? 0 : fs::file_size(entry);
        parentNode->children.push_back(node);
        tree.insert(node);
        if (entry.is_directory()) {
            mapFileSystem(entry.path(), node);
        }
    }
}

std::shared_ptr<INode> FileSystem::getCurrentDirectory() {
    return currentDirectory;
}

// Implementación de la clase FileSystem
FileSystem::FileSystem() : tree(3), nextId(1) {
    rootPath = fs::current_path() / "root";
    if (!fs::exists(rootPath)) {
        fs::create_directory(rootPath);
    }
    root = std::make_shared<INode>("root", true, nullptr);
    root->id = nextId++;
    currentDirectory = root;
    mapFileSystem(rootPath, root); // Mapear el sistema de archivos en la inicialización
}

void FileSystem::createFile(const std::string& name) {
    auto file = std::make_shared<INode>(name, false, currentDirectory);
    file->id = nextId++;
    currentDirectory->children.push_back(file);
    tree.insert(file);

    fs::path filePath = rootPath / name;
    std::ofstream ofs(filePath);
    ofs.close();
}

void FileSystem::createDirectory(const std::string& name) {
    auto dir = std::make_shared<INode>(name, true, currentDirectory);
    dir->id = nextId++;
    currentDirectory->children.push_back(dir);
    tree.insert(dir);

    fs::path dirPath = rootPath / name;
    fs::create_directory(dirPath);
}

void FileSystem::renameNode(const std::string& oldName, const std::string& newName) {
    for (auto& child : currentDirectory->children) {
        if (child->name == oldName) {
            fs::rename(rootPath / oldName, rootPath / newName);
            child->name = newName;
            return;
        }
    }
    std::cout << RED << "Nodo no encontrado!" << RESET << std::endl;
}

void FileSystem::deleteNode(const std::string& name) {
    for (auto it = currentDirectory->children.begin(); it != currentDirectory->children.end(); ++it) {
        if ((*it)->name == name) {
            fs::remove_all(rootPath / name);
            currentDirectory->children.erase(it);
            return;
        }
    }
    std::cout << RED << "Nodo no encontrado!" << RESET << std::endl;
}

void FileSystem::changePermissions(const std::string& name, const std::string& permissions) {
    try {
        for (auto& child : currentDirectory->children) {
            if (child->name == name) {
                // Convertir el string de permisos a un entero
                int perm = std::stoi(permissions);
                if (perm < 0 || perm > 7) {
                    std::cout << RED << "Permisos inválidos!" << RESET << std::endl;
                    return;
                }

                // Convertir el número a permisos
                std::string permStr = "---";
                if (perm & 4) permStr[0] = 'r';
                if (perm & 2) permStr[1] = 'w';
                if (perm & 1) permStr[2] = 'x';

                child->permissions = permStr + "r--r--"; // Permisos para user, group y others
                return;
            }
        }
        std::cout << RED << "Nodo no encontrado!" << RESET << std::endl;
    } catch (const std::invalid_argument&) {
        std::cout << RED << "Permisos inválidos: no es un número!" << RESET << std::endl;
    } catch (const std::out_of_range&) {
        std::cout << RED << "Permisos inválidos: fuera de rango!" << RESET << std::endl;
    }
}

void FileSystem::listFiles() {
    std::cout << GREEN << "Lista de archivos en el directorio actual:" << RESET << std::endl;
    for (const auto& child : currentDirectory->children) {
        std::cout <<  child->id << " " << (child->isDirectory ? "d" : "-") << child->permissions << " "
                  << child->name << " " << std::put_time(std::localtime(&child->creationTime), "%b %d %H:%M") << std::endl;
    }
}

void FileSystem::listFilesRecursively(std::shared_ptr<INode> node, int depth) {
    std::string indent(depth * 2, ' ');
    std::cout << indent << (node->isDirectory ? "d" : "-") << node->permissions << " " << node->name << " "
              << std::put_time(std::localtime(&node->creationTime), "%b %d %H:%M") << std::endl;
    for (const auto& child : node->children) {
        listFilesRecursively(child, depth + 1);
    }
}

void FileSystem::searchFileByName(const std::string& name) {
    std::cout << "Buscando archivo: " << name << std::endl;
    for (const auto& child : currentDirectory->children) {
        if (child->name == name) {
            std::cout << GREEN <<"Archivo encontrado: " << RESET << name << " INodo: " << child->id << std::endl;
            return;
        }
    }
    std::cout << RED <<"Archivo no encontrado!" << RESET << std::endl;
}

void FileSystem::showCurrentDirectory() {
    // Get the current absolute path of the current directory
    fs::path currentPath = fs::absolute(rootPath);
    
    // Display the current directory path
    std::cout << BLUE << "Directorio actual: " << YELLOW << currentPath.string() << RESET << std::endl;
}


void FileSystem::saveToFile(const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (ofs) {
        // Serializar el sistema de archivos
    }
    ofs.close();
}

void FileSystem::loadFromFile(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (ifs) {
        // Deserializar el sistema de archivos
    }
    ifs.close();
}

void FileSystem::changeDirectory(const std::string& name) {
    for (auto& child : currentDirectory->children) {
        if (child->name == name && child->isDirectory) {
            currentDirectory = child;
            rootPath /= name;
            return;
        }
    }
    std::cout << RED <<"Directorio no encontrado!" << RESET << std::endl;
}

void FileSystem::changeToParentDirectory() {
    if (currentDirectory->parent) {
        currentDirectory = currentDirectory->parent;
        rootPath = rootPath.parent_path();
    }
}

void FileSystem::displayHelp() {
    std::cout << "Comandos disponibles:" << std::endl;
    std::cout << "  createFile <nombre>     - Crear un nuevo archivo" << std::endl;
    std::cout << "  createDirectory <nombre> - Crear un nuevo directorio" << std::endl;
    std::cout << "  rename <antiguo> <nuevo> - Renombrar un archivo o directorio" << std::endl;
    std::cout << "  delete <nombre>         - Eliminar un archivo o directorio" << std::endl;
    std::cout << "  chmod <nombre> <permisos> - Cambiar permisos de un archivo o directorio" << std::endl;
    std::cout << "  ls                      - Listar archivos en el directorio actual" << std::endl;
    std::cout << "  ls -R                   - Listar archivos recursivamente" << std::endl;
    std::cout << "  find <nombre>           - Buscar un archivo por nombre" << std::endl;
    std::cout << "  cd <nombre>             - Cambiar de directorio" << std::endl;
    std::cout << "  cd..                    - Ir al directorio padre" << std::endl;
    std::cout << "  exit                    - Salir del programa" << std::endl;
    std::cout << "  help                    - Mostrar comandos disponibles" << std::endl;
}

int main() {
    FileSystem fs;
    std::string command;

    while (true) {
        fs.showCurrentDirectory();
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "exit") break;

        if (command.rfind("createFile ", 0) == 0) {
            fs.createFile(command.substr(11));
        } else if (command.rfind("createDirectory ", 0) == 0) {
            fs.createDirectory(command.substr(16));
        } else if (command.rfind("rename ", 0) == 0) {
            size_t space = command.find(' ', 7);
            std::string oldName = command.substr(7, space - 7);
            std::string newName = command.substr(space + 1);
            fs.renameNode(oldName, newName);
        } else if (command.rfind("delete ", 0) == 0) {
            fs.deleteNode(command.substr(7));
        } else if (command.rfind("chmod ", 0) == 0) {
            size_t space = command.find(' ', 6);
            std::string name = command.substr(6, space - 6);
            std::string permissions = command.substr(space + 1);
            fs.changePermissions(name, permissions);
        } else if (command == "ls") {
            fs.listFiles();
        } else if (command == "ls -R") {
            fs.listFilesRecursively(fs.getCurrentDirectory(), 0);
        } else if (command.rfind("find ", 0) == 0) {
            fs.searchFileByName(command.substr(5));
        } else if (command.rfind("cd ", 0) == 0) {
            fs.changeDirectory(command.substr(3));
        } else if (command == "cd..") {
            fs.changeToParentDirectory();
        } else if (command == "help") {
            fs.displayHelp();
        } else {
            std::cout << "Comando desconocido: " << command << std::endl;
        }
    }

    return 0;
}