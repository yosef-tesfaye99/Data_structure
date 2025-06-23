// ===== main.cpp =====

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include "sha1.h"

using namespace std;
namespace fs = filesystem;

// ---------------------
// Hash Function
// ---------------------
string simpleHash(const string& content) {
    return sha1(content);
}

// ---------------------
// INIT Command
// ---------------------
void initMiniGit() {
    string baseDir = ".minigit";

    if (fs::exists(baseDir)) {
        cout << "MiniGit repo already initialized.\n";
        return;
    }

    fs::create_directory(baseDir);
    fs::create_directory(baseDir + "/objects");
    fs::create_directory(baseDir + "/commits");
    fs::create_directory(baseDir + "/branches");

    ofstream headFile(baseDir + "/HEAD.txt");
    headFile << "ref: main\n";
    headFile.close();

    cout << "Initialized empty MiniGit repository in .minigit/\n";
}

// ---------------------
// ADD Command
// ---------------------
void addFileToStaging(const string& filename) {
    string repoPath = ".minigit";

    if (!fs::exists(repoPath)) {
        cout << "Repository not initialized. Run './minigit init' first.\n";
        return;
    }

    if (!fs::exists(filename)) {
        cout << "File not found: " << filename << "\n";
        return;
    }

    // Read file content
    ifstream inFile(filename);
    stringstream buffer;
    buffer << inFile.rdbuf();
    string content = buffer.str();
    inFile.close();

    // Generate hash
    string hash = simpleHash(content);
    string objectPath = repoPath + "/objects/" + hash;

    // Save blob if not already saved
    if (!fs::exists(objectPath)) {
        ofstream blobFile(objectPath);
        blobFile << content;
        blobFile.close();
    }

    // Record staging info
    ofstream indexFile(repoPath + "/index.txt", ios::app);
    indexFile << filename << " " << hash << "\n";
    indexFile.close();

    cout << "Added '" << filename << "' to staging area.\n";
}

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    return string(ctime(&currentTime));
}

void commitChanges(const string& message) {
    string repoPath = ".minigit";
    string indexPath = repoPath + "/index.txt";

    if (!fs::exists(repoPath) || !fs::exists(indexPath)) {
        cout << "Nothing to commit. Stage files first using './minigit add <file>'.\n";
        return;
    }

    // Read staged files
    ifstream indexFile(indexPath);
    stringstream stagedContent;
    stagedContent << indexFile.rdbuf();
    string staged = stagedContent.str();
    indexFile.close();

    if (staged.empty()) {
        cout << "No files staged for commit.\n";
        return;
    }

    // Get parent commit from HEAD
    string parentHash = "null";
    ifstream headFile(repoPath + "/HEAD.txt");
    string headContent;
    if (getline(headFile, headContent)) {
        if (headContent.rfind("ref:", 0) == 0) {
            string branchName = headContent.substr(5);
            ifstream branchFile(repoPath + "/branches/" + branchName + ".txt");
            if (branchFile.is_open()) {
                getline(branchFile, parentHash);
                branchFile.close();
            }
        } else {
            parentHash = headContent;  // detached HEAD
        }
    }
    headFile.close();

    // Generate commit hash
    string timestamp = getCurrentTimestamp();
    string commitContent = message + timestamp + staged;
    string commitHash = simpleHash(commitContent);

    // Write commit file
    ofstream commitFile(repoPath + "/commits/" + commitHash + ".txt");
    commitFile << "Commit: " << commitHash << "\n";
    commitFile << "Parent: " << parentHash << "\n";
    commitFile << "Date: " << timestamp;
    commitFile << "Message: " << message << "\n";
    commitFile << "Files:\n" << staged;
    commitFile.close();

    // Update HEAD if detached
    ifstream recheckHead(repoPath + "/HEAD.txt");
    getline(recheckHead, headContent);
    recheckHead.close();

    if (headContent.rfind("ref:", 0) == 0) {
        string branchName = headContent.substr(5);
        ofstream branchFile(repoPath + "/branches/" + branchName + ".txt");
        branchFile << commitHash << "\n";
        branchFile.close();
    } else {
        ofstream newHead(repoPath + "/HEAD.txt");
        newHead << commitHash << "\n";
        newHead.close();
    }

    // Clear staging area
    ofstream clearIndex(indexPath, ios::trunc);
    clearIndex.close();

    cout << "Committed with hash: " << commitHash << "\n";
}

void showCommitLog() {
    string repoPath = ".minigit";
    string headPath = repoPath + "/HEAD.txt";

    if (!fs::exists(headPath)) {
        cout << "Repository not initialized or no commits yet.\n";
        return;
    }

    ifstream headFile(headPath);
    string headContent;
    getline(headFile, headContent);
    headFile.close();

    string currentHash = headContent;

    // If HEAD is a branch ref, resolve it
    if (headContent.rfind("ref:", 0) == 0) {
        string branchName = headContent.substr(5); // skip "ref: "
        ifstream branchFile(repoPath + "/branches/" + branchName + ".txt");
        if (!branchFile.is_open()) {
            cout << "Error: Branch '" << branchName << "' not found.\n";
            return;
        }
        getline(branchFile, currentHash);
        branchFile.close();
    }

    // Traverse commit history
    while (currentHash != "null") {
        string commitPath = repoPath + "/commits/" + currentHash + ".txt";

        if (!fs::exists(commitPath)) {
            cout << "Error: Commit file missing for hash " << currentHash << "\n";
            break;
        }

        ifstream commitFile(commitPath);
        string line;
        string parentHash = "null";

        cout << "------------------------------\n";

        while (getline(commitFile, line)) {
            if (line.rfind("Commit:", 0) == 0 ||
                line.rfind("Date:", 0) == 0 ||
                line.rfind("Message:", 0) == 0) {
                cout << line << "\n";
            }

            if (line.rfind("Parent:", 0) == 0) {
                parentHash = line.substr(8); // skip "Parent: "
            }
        }

        commitFile.close();
        currentHash = parentHash;
    }

    cout << "------------------------------\n";
}

void createBranch(const string& branchName) {
    string repoPath = ".minigit";
    string headPath = repoPath + "/HEAD.txt";
    string branchPath = repoPath + "/branches/" + branchName + ".txt";

    if (!fs::exists(headPath)) {
        cout << "Repository not initialized.\n";
        return;
    }

    if (fs::exists(branchPath)) {
        cout << "Branch '" << branchName << "' already exists.\n";
        return;
    }

    ifstream headFile(headPath);
    string headContent;
    getline(headFile, headContent);
    headFile.close();

    string currentCommitHash;

    if (headContent.rfind("ref:", 0) == 0) {
        string currentBranch = headContent.substr(5); // e.g. "main"
        string branchFilePath = repoPath + "/branches/" + currentBranch + ".txt";

        // If the branch doesn't exist yet, we likely haven't committed yet
        if (!fs::exists(branchFilePath)) {
            cout << "Error: Cannot create branch before first commit on current branch '" 
                      << currentBranch << "'.\n";
            return;
        }

        ifstream branchFile(branchFilePath);
        getline(branchFile, currentCommitHash);
        branchFile.close();
    } else {
        // HEAD is a commit hash directly (e.g., after checkout)
        currentCommitHash = headContent;
    }

    // Create new branch pointing to the current commit
    ofstream newBranch(branchPath);
    newBranch << currentCommitHash << "\n";
    newBranch.close();

    cout << "Created new branch '" << branchName << "' at commit: " << currentCommitHash << "\n";
}

void checkoutTarget(const string& target) {
    string repoPath = ".minigit";
    string commitHash = target;
    bool isBranch = false;

    // Check if target is a branch name
    string branchPath = repoPath + "/branches/" + target + ".txt";
    if (fs::exists(branchPath)) {
        isBranch = true;
        ifstream branchFile(branchPath);
        getline(branchFile, commitHash);
        branchFile.close();
    }

    // Validate commit hash
    string commitFilePath = repoPath + "/commits/" + commitHash + ".txt";
    if (!fs::exists(commitFilePath)) {
        cout << "Commit not found for: " << target << "\n";
        return;
    }

    // Read commit file to get file list
    ifstream commitFile(commitFilePath);
    string line;
    bool inFilesSection = false;
    vector<pair<string, string>> files;

    while (getline(commitFile, line)) {
        if (line == "Files:") {
            inFilesSection = true;
            continue;
        }

        if (inFilesSection && !line.empty()) {
            istringstream ss(line);
            string filename, hash;
            ss >> filename >> hash;
            files.emplace_back(filename, hash);
        }
    }
    commitFile.close();

    // Overwrite working directory files
    for (const auto& [filename, hash] : files) {
        string blobPath = repoPath + "/objects/" + hash;
        if (fs::exists(blobPath)) {
            ifstream blobFile(blobPath);
            ofstream outFile(filename);
            outFile << blobFile.rdbuf();
            blobFile.close();
            outFile.close();
        } else {
            cout << "Missing blob for " << filename << "\n";
        }
    }

    // Update HEAD
    ofstream headFile(repoPath + "/HEAD.txt");
    if (isBranch) {
        headFile << "ref: " << target << "\n";
    } else {
        headFile << commitHash << "\n";  // detached
    }
    headFile.close();

    cout << "Checked out " << (isBranch ? "branch" : "commit") << ": " << target << "\n";
}

void mergeBranch(const string& targetBranch) {
    string repoPath = ".minigit";

    // 1. Get current branch from HEAD
    ifstream headFile(repoPath + "/HEAD.txt");
    string headContent;
    getline(headFile, headContent);
    headFile.close();

    if (headContent.rfind("ref:", 0) != 0) {
        cout << "You must be on a branch to perform a merge (not detached).\n";
        return;
    }

    string currentBranch = headContent.substr(5);
    string currentBranchPath = repoPath + "/branches/" + currentBranch + ".txt";
    string targetBranchPath = repoPath + "/branches/" + targetBranch + ".txt";

    if (!fs::exists(currentBranchPath) || !fs::exists(targetBranchPath)) {
        cout << "Error: One or both branches do not exist.\n";
        return;
    }

    // 2. Get commit hashes
    ifstream currentFile(currentBranchPath), targetFile(targetBranchPath);
    string currentHash, targetHash;
    getline(currentFile, currentHash);
    getline(targetFile, targetHash);
    currentFile.close(); targetFile.close();

    // 3. Find LCA
    set<string> visited;
    string walker = currentHash;

    while (walker != "null") {
        visited.insert(walker);

        ifstream commitFile(repoPath + "/commits/" + walker + ".txt");
        string line, parent = "null";
        while (getline(commitFile, line)) {
            if (line.rfind("Parent:", 0) == 0) {
                parent = line.substr(8);
                break;
            }
        }
        commitFile.close();
        walker = parent;
    }

    string lca = "null";
    walker = targetHash;
    while (walker != "null") {
        if (visited.count(walker)) {
            lca = walker;
            break;
        }

        ifstream commitFile(repoPath + "/commits/" + walker + ".txt");
        string line, parent = "null";
        while (getline(commitFile, line)) {
            if (line.rfind("Parent:", 0) == 0) {
                parent = line.substr(8);
                break;
            }
        }
        commitFile.close();
        walker = parent;
    }

    if (lca == "null") {
        cout << "No common ancestor. Cannot merge.\n";
        return;
    }

    cout << "Merging branch '" << targetBranch << "' into '" << currentBranch << "'\n";
    cout << "Lowest Common Ancestor: " << lca << "\n";

    // 4. Load file lists for each commit
    auto loadFiles = [&](const string& hash) {
        map<string, string> files;
        ifstream file(repoPath + "/commits/" + hash + ".txt");
        string line;
        bool inFiles = false;
        while (getline(file, line)) {
            if (line == "Files:") {
                inFiles = true;
                continue;
            }
            if (inFiles && !line.empty()) {
                istringstream ss(line);
                string filename, blob;
                ss >> filename >> blob;
                files[filename] = blob;
            }
        }
        file.close();
        return files;
    };

    auto lcaFiles = loadFiles(lca);
    auto currentFiles = loadFiles(currentHash);
    auto targetFiles = loadFiles(targetHash);

    // 5. Perform 3-way merge
    for (const auto& [filename, lcaBlob] : lcaFiles) {
        string blobA = currentFiles[filename];
        string blobB = targetFiles[filename];

        if (blobA == blobB || blobB.empty()) {
            continue; // same or deleted in target, no merge needed
        }

        if (blobA != lcaBlob && blobB != lcaBlob && blobA != blobB) {
            cout << "CONFLICT: both modified " << filename << "\n";

            // Write conflict marker file
            ofstream out(filename);
            out << "<<<<<<< current\n";
            ifstream a(repoPath + "/objects/" + blobA);
            out << a.rdbuf(); a.close();

            out << "\n=======\n";
            ifstream b(repoPath + "/objects/" + blobB);
            out << b.rdbuf(); b.close();

            out << "\n>>>>>>> " << targetBranch << "\n";
            out.close();
        } else {
            // Apply non-conflicting change
            string blob = blobB;
            ifstream in(repoPath + "/objects/" + blob);
            ofstream out(filename);
            out << in.rdbuf();
            in.close();
            out.close();
            cout << "Merged change from " << targetBranch << ": " << filename << "\n";
        }
    }

    cout << "Merge complete. Please resolve conflicts and commit the result.\n";
}

void diffCommits(const string& hash1, const string& hash2) {
    string repoPath = ".minigit";

    auto loadFiles = [&](const string& hash) -> map<string, string> {
        map<string, string> files;
        ifstream commitFile(repoPath + "/commits/" + hash + ".txt");
        if (!commitFile.is_open()) {
            cout << "Commit not found: " << hash << "\n";
            return files;
        }

        string line;
        bool inFiles = false;
        while (getline(commitFile, line)) {
            if (line == "Files:") {
                inFiles = true;
                continue;
            }
            if (inFiles && !line.empty()) {
                istringstream ss(line);
                string filename, blobHash;
                ss >> filename >> blobHash;
                files[filename] = blobHash;
            }
        }
        commitFile.close();
        return files;
    };

    auto files1 = loadFiles(hash1);
    auto files2 = loadFiles(hash2);

    for (const auto& [filename, blob1] : files1) {
        if (files2.find(filename) == files2.end()) continue; // only diff shared files

        string blob2 = files2[filename];
        ifstream file1(repoPath + "/objects/" + blob1);
        ifstream file2(repoPath + "/objects/" + blob2);

        vector<string> lines1, lines2;
        string line;

        while (getline(file1, line)) lines1.push_back(line);
        while (getline(file2, line)) lines2.push_back(line);

        file1.close(); file2.close();

        cout << "Diff: " << filename << "\n";

        size_t maxLines = max(lines1.size(), lines2.size());

        for (size_t i = 0; i < maxLines; ++i) {
            string a = i < lines1.size() ? lines1[i] : "";
            string b = i < lines2.size() ? lines2[i] : "";

            if (a != b) {
                if (!a.empty()) cout << "- " << a << "\n";
                if (!b.empty()) cout << "+ " << b << "\n";
            }
        }

        cout << "--------------------------\n";
    }
}

// ---------------------
// Main Function
// ---------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./minigit <command> [options]\n";
        return 1;
    }

    string command = argv[1];

    // ---------------------
    // INIT Command Handler
    // ---------------------
    if (command == "init") {
        initMiniGit();
    } 
    // ---------------------
    // ADD Command Handler
    // ---------------------
    else if (command == "add" && argc >= 3) {
        string filename = argv[2];
        addFileToStaging(filename);
    } 
    // ---------------------
    // COMMIT Command Handler
    // ---------------------
    else if (command == "commit" && argc >= 4 && string(argv[2]) == "-m") {
        string message = argv[3];
        commitChanges(message);
    } 
    // ---------------------
    // LOG Command Handler
    // ---------------------
    else if (command == "log") {
        showCommitLog();
    } 
    // ---------------------
    // BRANCH Command Handler
    // ---------------------
    else if (command == "branch" && argc >= 3) {
        string branchName = argv[2];
        createBranch(branchName);
    } 
    // ---------------------
    // CHECKOUT Command Handler
    // ---------------------
    else if (command == "checkout" && argc >= 3) {
        string target = argv[2];
        checkoutTarget(target);
    } 
    // ---------------------
    // MERGE Command Handler
    // ---------------------
    else if (command == "merge" && argc >= 3) {
        mergeBranch(argv[2]);
    } 
    // ---------------------
    // DIFF Command Handler
    // ---------------------
    else if (command == "diff" && argc >= 4) {
        diffCommits(argv[2], argv[3]);
    } 
    // ---------------------
    // Unknown Command Handler
    // ---------------------
    else {
        cout << "Unknown or incomplete command.\n";
    }

    return 0;
}