# Mini Version Control System

## Overview
The Mini Version Control System (Mini VCS) is a lightweight, C++-based tool designed to emulate core Git functionalities for managing file versions. It enables users to track changes, commit snapshots, and restore project states, leveraging SHA-1 hashing and compression for secure and efficient file storage. Built with modularity and robust error handling, this project demonstrates advanced C++ programming and version control concepts, ideal for developers exploring VCS internals or managing small-scale projects.

## Features
- **Repository Initialization**: Creates a `.git` directory to manage objects and references.
- **File Hashing**: Computes SHA-1 hashes for files, with optional storage as compressed blobs.
- **Content Inspection**: Retrieves file content, size, or type using SHA-1 hashes.
- **Directory Snapshots**: Captures directory structures as tree objects.
- **Tree Listing**: Displays tree contents with detailed metadata or names only.
- **File Staging**: Adds files to a staging area for commits.
- **Committing**: Records snapshots with customizable messages and metadata.
- **History Logging**: Shows commit history with SHA, timestamp, and messages.
- **State Restoration**: Reverts to specific commits, restoring file and directory states.

## Prerequisites
- **Operating System**: Linux (tested on Ubuntu)
- **Compiler**: g++ with C++17 support
- **Dependencies**: Standard C++ libraries (filesystem, STL), SHA-1 algorithm, compression utilities (no external libraries)

## Installation
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Samiksha2809/version_control_system.git
   cd version_control_system
