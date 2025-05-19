#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <openssl/sha.h>
#include <cstring>
#include <zlib.h>
#include <vector>
#include <algorithm>
#include <ctime>
#include <sstream> 
#include <iomanip> 
#include <regex>


using namespace std;

//structure for storing the index entry
struct IndexEntry {
    string path;
    string sha;
    string mode;
};



//structure for storing the commit info
struct Commit {
    string treeHash;
    string message;
    vector<string> parentHashes;
};

const int CHUNK_SIZE = 32; 


//calculating the sha1 hash using the open-ssl library
string calculateSHA1(const string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    ostringstream result;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        result << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }

    return result.str();
}


// Reading the filecontent from the filepath
string readFileContent(const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file: " + filePath);
    }

    ostringstream contentStream;
    contentStream << file.rdbuf();
    return contentStream.str();
}


// Computing the hash using SHA1
string computeGitHash(const string& filePath) {
    //cout << "Reaching Here" << endl;
    // Read the file content
    string fileContent = readFileContent(filePath);

    // Create the header
    //cout << fileContent.size()+"Hererererer"<< endl;
    string header = "blob " + to_string(fileContent.size()) + '\0';
     //cout << header << endl;
    // Combine the header and file content
    string fullContent = header + fileContent;

    // Calculate the SHA-1 hash of the combined content
    return calculateSHA1(fullContent);
}

filesystem::path getFilePathFromSHA(const string &sha) {
    cout << "sha is"<<sha << endl;
    //cout << "length is"+sha.length() << endl;
    if (sha.length() != 40) {
        //cout << "being terminated form here" << endl;
        throw invalid_argument("Invalid SHA format. SHA must be 40 characters long.");
    }
    return filesystem::path(".git") / "objects" / sha.substr(0, 2) / sha.substr(2);
}


void initializeZlibStream(z_stream &zs) {
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) {
        throw runtime_error("Failed to initialize zlib for decompression");
    }
}


//Function for writing the objects in .git/objects folder
void writeCompressedObject(const string &filepath, const string &hash) {
    //cout << "error hereee" << endl;
        if (hash.empty()) {
        throw runtime_error("hash is empty, cannot proceed with creating directories.");
    }

    string dirPath = ".git/objects/" + hash.substr(0, 2);
    string file_Path = dirPath + "/" + hash.substr(2);

    // Ensure directory exists
    filesystem::create_directories(dirPath);

    // Read file content into a string
    string fileContent = readFileContent(filepath);

    // Create the header and combine with the file content
    string header = "blob " + to_string(fileContent.size()) + '\0';
    string fullContent = header + fileContent;

    // Prepare zlib stream
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw runtime_error("Failed to initialize zlib for compression");
    }

    // Set up input data for compression
    zs.next_in = reinterpret_cast<Bytef*>(fullContent.data());
    zs.avail_in = fullContent.size();

    vector<unsigned char> compressedData(1024);
    zs.next_out = compressedData.data();
    zs.avail_out = compressedData.size();

    // Open file to write compressed data
    ofstream outputFile(file_Path, ios::binary);
    if (!outputFile) {
        throw runtime_error("Failed to create object file at: " + file_Path);
    }

    // Compress and write data
    while (zs.avail_in > 0) {
        int status = deflate(&zs, Z_FINISH);
        if (status == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw runtime_error("Error during compression");
        }

        size_t bytesCompressed = compressedData.size() - zs.avail_out;
        outputFile.write(reinterpret_cast<char*>(compressedData.data()), bytesCompressed);

        // Reset output buffer
        zs.next_out = compressedData.data();
        zs.avail_out = compressedData.size();
    }

    deflateEnd(&zs);
    outputFile.close();
}



//Decompresses the objects in chunk-wise manner/ used for catfile command
void decompressGitObjectChunkwise(const string &filepath,bool printContent, bool printSize, bool printType) {
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Unable to open file: " + filepath);
    }

    z_stream zs;
    initializeZlibStream(zs);

    unsigned char inputBuffer[CHUNK_SIZE];
    unsigned char outputBuffer[CHUNK_SIZE];
     bool headerParsed = false;
    string header;

    try {
        while (file.read(reinterpret_cast<char *>(inputBuffer), CHUNK_SIZE) || file.gcount() > 0) {
            zs.avail_in = static_cast<int>(file.gcount());
            zs.next_in = inputBuffer;

            do {
                zs.avail_out = sizeof(outputBuffer);
                zs.next_out = outputBuffer;

                int ret = inflate(&zs, Z_NO_FLUSH);
                if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                    inflateEnd(&zs);
                    throw runtime_error("Decompression error occurred");
                }

                int bytesDecompressed = sizeof(outputBuffer) - zs.avail_out;
               // cout << bytesDecompressed << endl;

                // Print only after the first null byte
                for (int i = 0; i < bytesDecompressed; ++i) {
                   //cout.write(reinterpret_cast<char *>(&outputBuffer[i]), 1);
                 if(!headerParsed){
                    if (outputBuffer[i] == '\0') {
                        //cout << "Null character found " << endl;
                        // Start printing after the first null byte
                        headerParsed = true;
                        continue;
                    }
                    header += outputBuffer[i];
                 } else{
                    if(printContent){
                    cout.write(reinterpret_cast<char *>(&outputBuffer[i]), 1);
                    }
                 }
                }
            } while (zs.avail_out == 0); // Continue decompressing until the output buffer is full
        }

        //cout << header + "heuuuuu"<< endl;
        if (headerParsed) {
            size_t spacePos = header.find(' ');
            if (spacePos != string::npos) {
                string objectType = header.substr(0, spacePos); // Extract object type
                string sizeStr = header.substr(spacePos + 1);   // Extract size

                // Print object type if requested
                if (printType) {
                    cout << "Object Type: " << objectType << endl;
                }

                // Print object size if requested
                if (printSize) {
                    cout << "Object Size: " << sizeStr << endl;
                }
            }
        }
    } catch (...) {
        inflateEnd(&zs);
        throw; // Rethrow the exception
    }

    inflateEnd(&zs);
}

//This write-tree corresponds to making shas and storing the objects in the objects-store recursively
string writeTreeRecursive(const string dirPath){
    namespace fs = filesystem;
    vector<pair<string,string>> entries;
    string mode;
    string hash = "";
    string githash;
    for (const auto& entry : fs::directory_iterator(dirPath)){
        string name = entry.path().filename().string();
        if(name == ".git")
        continue;

        if(entry.is_directory()){
            mode ="040000";
            hash = writeTreeRecursive(entry.path().string());

            

        }
        else if (entry.is_regular_file()){
            mode = "100644";
            hash = computeGitHash(entry.path().string());
            //cout << "Here " << hash << endl;
            try {
         string objectDir = ".git/objects/" + hash.substr(0, 2);
            string objectFile = objectDir + "/" + hash.substr(2);

            // Check if the object already exists in the object store
            if (!filesystem::exists(objectFile)) {
                // Write the file content to the object store since it doesn't exist
                writeCompressedObject(entry.path().string(), hash);
            }
        //cout << "Compressed object written successfully.\n";
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << '\n';
    }
    }

    if(!hash.empty()){
        //cout << "hereeee" << endl;
        string binary_sha;
        string entry_data;
            for (size_t i = 0; i < hash.length(); i += 2)
            {
                string byte_string = hash.substr(i, 2);
                //cout << byte_string << endl;
                char byte = static_cast<char>(stoi(byte_string, nullptr, 16));
                binary_sha.push_back(byte);
            }
            if(mode == "100644")
             entry_data = mode + " " + name + '\0' + binary_sha;
            else if(mode == "040000")
             entry_data = mode + " " + name + '\0' + binary_sha;
            //cout << "name is" << name << endl;
            //cout << entry_data << endl;
            //cout << "Successfully written entry data " << endl; 
            entries.push_back({name, entry_data});
    }
        
    }


    //if the directory is empty it gives a default hash
     if (entries.empty()) {
        // Handle empty directory case
        return "4b825dc642cb6a0b0530b11d3f600bfb1f6f41c0"; // SHA-1 for an empty tree
    }



    sort(entries.begin(),entries.end());
    string treeContent;
    for(auto &it : entries){
        treeContent += it.second;
    }
    string tree_store = "tree " + to_string(treeContent.size()) + '\0' + treeContent;
    unsigned char hasharr[20];
    SHA1((unsigned char *)tree_store.c_str(), tree_store.size(), hasharr);
    string treeSha;
    for (int i = 0; i < 20; i++)
    {
        stringstream sstream;
        sstream << hex << setfill('0') << setw(2) << static_cast<int>(hash[i]);
        treeSha += sstream.str();
    }
      //cout << "tree sha is "<< treeSha << endl;

       if (treeSha.empty()) {
        throw runtime_error("treeSha is empty, cannot proceed with creating directories.");
    }
       string tree_dir = ".git/objects/" + treeSha.substr(0, 2);
       filesystem::create_directory(tree_dir);
       string tree_filepath = tree_dir + "/" + treeSha.substr(2);

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK)
    {
        throw(runtime_error("deflateInit failed while compressing."));
    }
    zs.next_in = (Bytef *)tree_store.c_str();
    zs.avail_in = tree_store.size();
    int ret;
    char outBuffer[32768];
    string outstring;
     do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outBuffer);
        zs.avail_out = sizeof(outBuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out)
        {
            // TODO: check other approaches
            outstring.insert(outstring.end(), outBuffer, outBuffer + zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END)
    {
        throw(runtime_error("Exception during zlib compression: " + to_string(ret)));
    }
    ofstream outfile(tree_filepath, ios::binary);
    outfile.write(outstring.c_str(), outstring.size());
    outfile.close();
    return treeSha;


}



//Function for handling the ls-tree command
void ls_tree(const string &value, const bool name_only_flag)
{
    const string dirname = value.substr(0, 2);
    const string tree_sha = value.substr(2);
    const string tree_path = ".git/objects/" + dirname + "/" + tree_sha;
    string entry;

    ifstream file(tree_path, ios::binary);
    if (!file.is_open())
    {
        throw runtime_error("Could not open file: " + tree_path);
    }

    // Read and decompress the file content
    stringstream buffer;
    buffer << file.rdbuf();
    string blob_data = buffer.str();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK)
    {
        throw runtime_error("inflateInit failed while decompressing.");
    }

    zs.next_in = (Bytef *)blob_data.c_str();
    zs.avail_in = blob_data.size();
    int ret = 0;
    char outputBuffer[32768];
    string outputString;
    //cout << "loop1" <<endl;
    do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outputBuffer);
        zs.avail_out = sizeof(outputBuffer);
        ret = inflate(&zs, 0);
        if (outputString.size() < zs.total_out)
        {
            outputString.append(outputBuffer, zs.total_out - outputString.size());
        }
    } while (ret == Z_OK);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END)
    {
        throw runtime_error("Exception during zlib decompression: " + to_string(ret));
    }


     //cout << "herer" << endl;
     // Parse the decompressed tree data
    vector<string> entries;
    string treeObject = outputString.substr(outputString.find('\0') + 1);
    size_t pos = 0;

   
    while (pos < treeObject.size())
{
    size_t space = treeObject.find(' ', pos);
    if (space == string::npos) break; // Check for end of string
    string mode = treeObject.substr(pos, space - pos);
    pos = space + 1;

    size_t nullp = treeObject.find('\0', pos);
    if (nullp == string::npos) break; // Check for end of string
    string name = treeObject.substr(pos, nullp - pos);
    pos = nullp + 1; // Move past the null character

    if (pos + 20 > treeObject.size()) break; // Ensure there are enough bytes for SHA-1
    string sha1;
    for (int i = 0; i < 20; ++i)
    {
        sha1 += "0123456789abcdef"[(unsigned char)treeObject[pos + i] >> 4];
        sha1 += "0123456789abcdef"[(unsigned char)treeObject[pos + i] & 0xf];
    }
    pos += 20; // Move past the SHA-1

     string type;
    if (mode == "040000") {
        type = "tree";
    } else if (mode == "100644") {
        type = "blob";
    }

     if (name_only_flag) {
        entries.push_back(name);
    } else {
        entry = mode + "\t" + type + "\t" + sha1 + "\t" + name;  // Change the order here
        entries.push_back(entry);
    }
}


    //sort(entries.begin(), entries.end());
    sort(entries.begin(), entries.end(), [](const string& a, const string& b) {
    size_t posA = a.find_last_of('\t');
    size_t posB = b.find_last_of('\t');
    return a.substr(posA + 1) < b.substr(posB + 1); // Compare based on the name
});
    for (const auto &entry : entries)
    {
        cout << entry << "\n";
    }
}




// Function to read the current index from the .git/index file
unordered_map<string, IndexEntry> readIndex() {
    unordered_map<string, IndexEntry> indexMap;
    ifstream indexFile(".git/index");
    
    if (!indexFile) {
        // If the index file does not exist, return an empty map
        return indexMap;
    }

    string line;
    while (getline(indexFile, line)) {
        IndexEntry entry;
        size_t firstSpace = line.find(' ');
        size_t secondSpace = line.find(' ', firstSpace + 1);
        
        if (firstSpace != string::npos && secondSpace != string::npos) {
            entry.mode = line.substr(0, firstSpace);
            entry.sha = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            entry.path = line.substr(secondSpace + 1);
            indexMap[entry.path] = entry; // Use the file path as the key
        }
    }

    return indexMap;
}

// Function to write the updated index back to .git/index
void writeIndex(const unordered_map<string, IndexEntry>& indexMap) {
    ofstream indexFile(".git/index", ios::binary);
    if (!indexFile) {
        cerr << "Failed to open .git/index for writing" << endl;
        return;
    }

    for (const auto& [path, entry] : indexMap) {
        indexFile << entry.mode << " " << entry.sha << " " << entry.path << "\n";
    }

    indexFile.close();
}


// Function to add files to the index
void addToIndex(const vector<string>& files) {
    unordered_map<string, IndexEntry> indexMap = readIndex();
     IndexEntry entry;
    for (const auto& path : files) {
        if (filesystem::exists(path)) {
            if (filesystem::is_directory(path)) {
                // If it's a directory, process it recursively
                string treeSha = writeTreeRecursive(path);
              cout << "tree sha is"<<treeSha <<endl;
                entry.path = path;
                entry.sha = treeSha; // Use the SHA of the directory tree
                entry.mode = "040000"; // Mode for directory

                // Add or update the entry in the index
                indexMap[entry.path] = entry;
            } else if (filesystem::is_regular_file(path)) {
               
                entry.path = path;
                entry.sha = computeGitHash(path);
                entry.mode = "100644"; // Assuming regular files

                // Construct the object file path based on the SHA
                string objectDir = ".git/objects/" + entry.sha.substr(0, 2);
                string objectFile = objectDir + "/" + entry.sha.substr(2);

                // Check if the object already exists in the object store
                if (!filesystem::exists(objectFile)) {
                    // Write the file content to the object store since it doesn't exist
                    writeCompressedObject(path, entry.sha);
                }

                // Add or update the entry in the index
                indexMap[entry.path] = entry;
            } else {
                cerr << "Not a regular file or directory: " << path << endl;
            }
        } else {
            cerr << "File or directory not found: " << path << endl;
        }
    }

    // Write updated index back to the file
    writeIndex(indexMap);
}





// Function for handling the add command
void handleAddCommand(int argc, char* argv[]) {
    vector<string> files;

    if (argc == 3 && string(argv[2]) == ".") {
        // Add all files in the current directory
        for (const auto& entry : filesystem::directory_iterator(".")) {
           
 if (entry.is_directory() && entry.path().filename() == ".git") {
                continue; // Skip the .git directory
            }

                files.push_back(entry.path().string());
            
        }
    } else {
        for (int i = 2; i < argc; ++i) {
            files.push_back(argv[i]);
        }
    }

    addToIndex(files);
}

//Function to get the correct timestamp
string getCurrentTimestamp() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buf;
}

// Function to create a commit object
string createCommitObject(const string& treeHash, const string& parentHash, const string& author, const string& commitMessage, const string& timestamp) {
   
    //cout << timestamp << endl;
    string commitData = "tree " + treeHash + "\n" +
                         "parent " + parentHash + "\n" +
                         "author " + author + " " + timestamp + "\n" +
                         "committer " + author + " " + timestamp + "\n\n" +
                         commitMessage + "\n";
    string commit_obj = "commit " + to_string(commitData.size()) + '\0' + commitData;

    // Hash the commit data
    return commit_obj;
}

// Function to store the commit object
void writeCommitObject(const string& commitHash, const string& commitData) {
    //cout << commitData << endl;
    string dirPath = ".git/objects/" + commitHash.substr(0, 2);
    string filePath = dirPath + "/" + commitHash.substr(2);

    // Ensure directory exists
    filesystem::create_directories(dirPath);
    

    // Preparing a zlib stream for compression
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw runtime_error("Failed to initialize zlib for compression");
    }

    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(commitData.c_str()));
    zs.avail_in = commitData.size();

    vector<unsigned char> compressedData(1024);
    zs.next_out = compressedData.data();
    zs.avail_out = compressedData.size();

    // Open file to write compressed data
    ofstream outputFile(filePath, ios::binary);
    if (!outputFile) {
        throw runtime_error("Failed to create commit file at: " + filePath);
    }

    // Compress and write data
    while (zs.avail_in > 0) {
        int status = deflate(&zs, Z_FINISH);
        if (status == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw runtime_error("Error during compression");
        }

        size_t bytesCompressed = compressedData.size() - zs.avail_out;
        outputFile.write(reinterpret_cast<char*>(compressedData.data()), bytesCompressed);

        // Reset output buffer
        zs.next_out = compressedData.data();
        zs.avail_out = compressedData.size();
    }

    deflateEnd(&zs);
    outputFile.close();
}

// Function to update the reference to the current branch
void updateCurrentBranch(const string& commitHash) {
    ofstream branchFile(".git/refs/heads/main");
    if (!branchFile) {
        throw runtime_error("Failed to open branch file for updating");
    }
    branchFile << commitHash;
    branchFile.close();
}

void writeCommitLog(const string& commitSHA, const string& parentSHA, 
                    const string& message, const string& timestamp,
                    const string& committer) {
    ofstream logFile(".git/commit.log", ios::app);
    if (!logFile) {
        throw runtime_error("Failed to open log file for updating");
    }
    //cout << timestamp << endl;
    logFile << "Commit SHA: " << commitSHA << endl;
    logFile << "Parent SHA: " << (parentSHA.empty() ? "None" : parentSHA) << endl;
    logFile << "Message: " << message << endl;
    logFile << "Timestamp: " << timestamp << endl;
    logFile << "Committer: " << committer << endl;
    logFile << "----------------------------------------" << endl;

    logFile.close();
}






vector<tuple<string, string, string>> readIndexFile(const string& indexPath) {
    vector<tuple<string, string, string>> entries; // corresponds to (mode, hash, filepath)
    ifstream indexFile(indexPath);
    string line;

    while (getline(indexFile, line)) {
        // Assuming the format is: mode hash filepath
        istringstream iss(line);
        string mode, hash, filepath;
        if (iss >> mode >> hash >> filepath) {
            entries.emplace_back(mode, hash, filepath);
        }
    }

    return entries;
}

// Function to write the tree from the index
string writeTreeFromIndex(const string& indexPath) {
    vector<tuple<string, string, string>> entries = readIndexFile(indexPath);
    vector<pair<string, string>> treeEntries;

    for (const auto& entry : entries) {
        string mode = get<0>(entry);
        string hash = get<1>(entry);
        string name = get<2>(entry);

        // Convert hash from hex to binary
        string binary_hash;
        for (size_t i = 0; i < hash.length(); i += 2) {
            string byte_string = hash.substr(i, 2);
            char byte = static_cast<char>(stoi(byte_string, nullptr, 16));
            binary_hash.push_back(byte);
        }

        string entry_data = mode + " " + name + '\0' + binary_hash;
        treeEntries.push_back({name, entry_data});
    }

    // Sort entries by name
    sort(treeEntries.begin(), treeEntries.end());

    // Construct tree content
    string treeContent;
    for (const auto& it : treeEntries) {
        treeContent += it.second;
    }

    string tree_store = "tree " + to_string(treeContent.size()) + '\0' + treeContent;
    unsigned char hasharr[20];
    SHA1((unsigned char*)tree_store.c_str(), tree_store.size(), hasharr);

    // Convert hash array to hex string
    string treeSha;
    for (int i = 0; i < 20; i++) {
        stringstream sstream;
        sstream << hex << setfill('0') << setw(2) << static_cast<int>(hasharr[i]);
        treeSha += sstream.str();
    }

    if (treeSha.empty()) {
        throw runtime_error("treeSha is empty, cannot proceed with creating directories.");
    }

    // Define the object path in the object store
    string tree_dir = ".git/objects/" + treeSha.substr(0, 2);
    string tree_filepath = tree_dir + "/" + treeSha.substr(2);

    // Check if the object already exists
    if (filesystem::exists(tree_filepath)) {
        // If it exists, return the existing hash
        return treeSha;
    }

    // Create the directory if it doesn't exist
    filesystem::create_directory(tree_dir);

    // Compress and write the tree object
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw runtime_error("deflateInit failed while compressing.");
    }
    zs.next_in = (Bytef*)tree_store.c_str();
    zs.avail_in = tree_store.size();
    int ret;
    char outBuffer[32768];
    string outstring;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
        zs.avail_out = sizeof(outBuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out) {
            outstring.insert(outstring.end(), outBuffer, outBuffer + zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) {
        throw runtime_error("Exception during zlib compression: " + to_string(ret));
    }

    ofstream outfile(tree_filepath, ios::binary);
    outfile.write(outstring.c_str(), outstring.size());
    outfile.close();

    return treeSha;
}








// The commit function
void commit(const string& message) {
    // Read the index (staging area)
    auto indexMap = readIndex();
    if (indexMap.empty()) {
        cerr << "No changes to commit." << endl;
        return;
    }

    // Create a tree object from the index
    //string treeHash = writeTreeRecursive(".");
    string treeHash = writeTreeFromIndex(".git/index");
     string timestamp = getCurrentTimestamp();

    // Get the parent commit hash (if any)
    string parentHash;
    ifstream branchFile(".git/refs/heads/main");
    if (branchFile.is_open()) {
        getline(branchFile, parentHash);
        branchFile.close();
    }

    // Get author information (could be hardcoded or configurable)
    string author = "Samiksha Sharma <samiksha@example.com>"; // Replace with actual author info

    // Create the commit object
    string commitData = createCommitObject(treeHash, parentHash, author, message,timestamp);
    string commitHash = calculateSHA1(commitData); // This should be the SHA-1 hash of commitData

    // Write the commit object to the object store
    writeCommitObject(commitHash, commitData);

    // Update the branch reference to point to the new commit
    updateCurrentBranch(commitHash);


    string commitSHA = commitHash ;
    string parentSHA = parentHash;
    string timeStamp = timestamp;
    string committer = author;

    writeCommitLog(commitSHA, parentSHA, message, timeStamp, committer);



    cout << "Committed with hash: " << commitHash << endl;
}

string binaryToHex(const string& binaryData) {
    ostringstream hexStream;
    for (unsigned char byte : binaryData) {
        hexStream << hex << setw(2) << setfill('0') << static_cast<int>(byte);
    }
    return hexStream.str();
}




string decompressData(const string& compressedData) {
    z_stream zs;
    initializeZlibStream(zs);
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.data()));
    zs.avail_in = compressedData.size();

    const size_t bufferSize = 4096;
    char outputBuffer[bufferSize];
    string decompressedData;

    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outputBuffer);
        zs.avail_out = bufferSize;
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&zs);
            throw runtime_error("Decompression error occurred");
        }
        decompressedData.append(outputBuffer, bufferSize - zs.avail_out);
    } while (ret == Z_OK);
    inflateEnd(&zs);

    return decompressedData;
}


string readFileContentFromSHA(const string& sha1) {
    string filePath = ".git/objects/" + sha1.substr(0, 2) + "/" + sha1.substr(2);
    
    if (!filesystem::exists(filePath)) {
        cerr << "Error: Object " << sha1 << " does not exist." << endl;
        return "";
    }

    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file: " + filePath);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    // Decompress the content
    string decompressedContent = decompressData(content);

    // The actual content starts after the null character
    size_t nullCharPos = decompressedContent.find('\0');
    if (nullCharPos != string::npos) {
        return decompressedContent.substr(nullCharPos + 1); // Getting the content after the null character
    }
    
    return ""; // Return empty if no null character found
}

// Function to restore a tree
void checkoutTree(const string& treeSHA) {
    string treePath = ".git/objects/" + treeSHA.substr(0, 2) + "/" + treeSHA.substr(2);

    if (!filesystem::exists(treePath)) {
        cerr << "Error: Tree " << treeSHA << " does not exist." << endl;
        return;
    }

    // Read the tree object
    ifstream treeFile(treePath, ios::binary);
    if (!treeFile.is_open()) {
        throw runtime_error("Could not open tree file: " + treePath);
    }

    stringstream buffer;
    buffer << treeFile.rdbuf();
    string treeData = buffer.str();

    // Decompress the tree data
    string decompressedData = decompressData(treeData);

    // Parse and restore files/directories from the tree content
    size_t pos = 0;
    while (pos < decompressedData.size()) {
        size_t space = decompressedData.find(' ', pos);
        if (space == string::npos) break;

        string mode = decompressedData.substr(pos, space - pos);
        pos = space + 1;

        size_t nullChar = decompressedData.find('\0', pos);
        if (nullChar == string::npos) break;

        string name = decompressedData.substr(pos, nullChar - pos);
        pos = nullChar + 1;

        string sha1b = decompressedData.substr(pos, 20);
        string sha1 = binaryToHex(sha1b);
        pos += 20; // Move past SHA-1

        // Create directory or file
        if (mode == "040000") {
            filesystem::create_directory(name);
        } else if (mode == "100644") {
            string fileContent = readFileContentFromSHA(sha1);
           // cout << fileContent << endl;
            ofstream outFile(name, ios::binary);
            if (!outFile) {
                cerr << "Error opening file for writing: " << name << endl;
                return;
            }
            outFile << fileContent; // Write content to the file
            outFile.close();
        }
    }

    cout << "Checked out tree " << treeSHA << endl;
}

// Function to checkout a commit
void checkout(const string& commitSHA) {
    string commitPath = ".git/objects/" + commitSHA.substr(0, 2) + "/" + commitSHA.substr(2);

    if (!filesystem::exists(commitPath)) {
        cerr << "Error: Commit " << commitSHA << " does not exist." << endl;
        return;
    }

    // Read the commit object
    ifstream commitFile(commitPath, ios::binary);
    if (!commitFile.is_open()) {
        throw runtime_error("Could not open commit file: " + commitPath);
    }

    stringstream buffer;
    buffer << commitFile.rdbuf();
    string commitData = buffer.str();

    // Decompress the commit data
    string decompressedData = decompressData(commitData);

    // Extract the tree SHA from the commit data
    regex treeRegex(R"(tree\s+([a-f0-9]{40}))");
    smatch match;

    if (regex_search(decompressedData, match, treeRegex) && match.size() > 1) {
        string treeSHA = match[1];
        checkoutTree(treeSHA);
    } else {
        cerr << "Error: No tree SHA found in commit." << endl;
    }
}







int main(int argc, char *argv[])
{
    // Flush after every cout / cerr
    cout << unitbuf;
    cerr << unitbuf;

   
    if (argc < 2) {
        cerr << "Command not given.\n";
        return EXIT_FAILURE;
    }
    
    string command = argv[1];
    
    if (command == "init") {
        try {
            filesystem::create_directory(".git");
            filesystem::create_directory(".git/objects");
            filesystem::create_directory(".git/refs");
            filesystem::create_directory(".git/hooks");
            filesystem::create_directory(".git/branches");
            filesystem::create_directory(".git/info");
            filesystem::create_directory(".git/refs/heads");
            
            ofstream mainFile(".git/refs/heads/main");
            ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }

            ofstream configfile(".git/config");
            if (configfile.is_open()){
                configfile << " [core]\n";
                configfile << " repositoryformatversion = 0\n";
                configfile << " filemode = true\n";
                configfile << " bare = false\n";
                configfile << " logallrefupdates = true\n";
                configfile.close();
            }
            else{
                cerr << "Failed to create .git/config file\n";
            }

ofstream descriptionfile(".git/description");
            if (descriptionfile.is_open()){
                descriptionfile << " Unnamed repository; edit this file 'description' to name the repository.\n";
                descriptionfile.close();
            }
            else{
                cerr << "Failed to create .git/description file\n";
            }
    
            cout << "Initialized git directory\n";
        } catch (const filesystem::filesystem_error& e) {
            cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } 
    else if (command == "hash-object"){
        if (argc < 3) {
        cerr << "Usage: hash-object [-w] <file_path>\n";
        return EXIT_FAILURE;
        }

         bool writeObject = false;
         string filepath;
        if (argc == 4 && string(argv[2]) == "-w") {
        writeObject = true;
        filepath = argv[3];
    } else if (argc == 3) {
        filepath = argv[2];
    } else {
        cerr << "Usage: hash-object [-w] <file_path>\n";
        return EXIT_FAILURE;
    }
    string githash = computeGitHash(filepath);
     cout << githash << "\n";

     if(writeObject){
     
    try {
        writeCompressedObject(filepath, githash);
        //cout << "Compressed object written successfully.\n";
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << '\n';
    }
        
     }
        

    }
    else if(command == "cat-file"){
        if(argc < 4 || (string(argv[2]) != "-p" && string(argv[2]) != "-s" && string(argv[2]) != "-t") ){
        cerr << "Invalid Argument : Missing parameter: [-p/-s/-t] <hash>\n";
        return EXIT_FAILURE;
        }
    string hash = argv[3];
    string flag = string(argv[2]);
    try {
        auto filePath = getFilePathFromSHA(hash);
        if(flag == "-p")
        decompressGitObjectChunkwise(filePath.string(),true,false,false);
        else if(flag == "-s")
        decompressGitObjectChunkwise(filePath.string(),false,true,false);
        else if(flag == "-t")
        decompressGitObjectChunkwise(filePath.string(),false,false,true);
    } catch (const exception &ex) {
        cerr << "Error: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
    }
    else if(command == "write-tree"){
    string final_treehash = writeTreeRecursive(".");
    if(final_treehash.empty()){
        cerr << "Error in writing the tree object" << endl;
        return EXIT_FAILURE;
    }
    cout << final_treehash << endl;
    }
    else if(command == "ls-tree"){
     if(argc < 3){
        cerr << "Usage: ./mygit ls-tree [--name-only] <tree_sha>" << endl;
        return EXIT_FAILURE;
     }
       bool nameOnly = (argc >= 4 && string(argv[2]) == "--name-only");
      string tree_hash = argv[nameOnly ? 3 : 2];
       if (tree_hash.length() != 40) {
        cerr << "The given tree hash-length is invalid. It must be of 40 characters.\n";
        return EXIT_FAILURE;
    }
    ls_tree(tree_hash,nameOnly);

    }
    else if(command == "add"){
        handleAddCommand(argc, argv);

    }
     else if (command == "commit") {
        string commitMessage = " This is a default commit message"; // Default message

        // Check for the -m flag
        if (argc > 2 && string(argv[2]) == "-m") {
            if (argc < 4) {
                cerr << "Usage: commit -m <message>" << endl;
                return EXIT_FAILURE;
            }
            commitMessage = argv[3]; // Get the commit message
        } else if (argc > 2) {
            cerr << "Usage: commit -m <message>" << endl;
            return EXIT_FAILURE;
        }

        // Call the commit function with the determined message
        commit(commitMessage);
    }
    else if(command == "log"){
     ifstream inputFile(".git/commit.log");
     if (!inputFile.is_open()) {
        cerr << "Error opening file!" << endl;
        return 1;  // Exit with error code
    }
    string line;
    while (getline(inputFile, line)) {
        cout << line << endl;  
    }

    inputFile.close(); 

    }
    else if (command == "checkout") {
    if (argc == 3) {
        checkout(argv[2]);
    } else {
        cerr << "Usage: ./mygit checkout <commit_sha>" << endl;
    }
}
    
    else {
        cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
