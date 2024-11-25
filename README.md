# C++ Algorithm on AWS Lambda Custom Runtime

This repository contains a C++ algorithm designed to run on AWS Lambda using a custom runtime. The project uses the `nlohmann/json` library for JSON parsing. Follow the instructions below to set up the development environment on Ubuntu.

---

## Prerequisites

Ensure the following are installed on your system:

1. **g++** (Version 13 or later)
2. **CMake** (Optional, for advanced builds)
3. **nlohmann/json** library

---

## Environment Setup


### Step 1: Install g++
```bash
sudo apt install g++-13
```

### Step 2: Install the Default cmake Package
```bash
sudo apt update
sudo apt install cmake
```

### Step 3: Install nlohmann/json
```bash
sudo apt install nlohmann-json3-dev
```

## C++ Lambda Runtime Setup

### Install the CURL Development Libraries

```bash
sudo apt update
sudo apt install libcurl4-openssl-dev
```

### Download and compile the runtime

```bash
cd ~ 
git clone https://github.com/awslabs/aws-lambda-cpp.git
cd aws-lambda-cpp
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=~/out
make
make install
```

If your system need administrative permission then use

```bash
sudo make
sudo make install
```

### Create your C++ function

#### 1.Create a new directory for this project anywhere
```bash
mkdir hello-cpp-world
cd hello-cpp-world
```

#### 2.In that directory, create a file named main.cpp with the following content:

```
// main.cpp
#include <aws/lambda-runtime/runtime.h>

using namespace aws::lambda_runtime;

invocation_response my_handler(invocation_request const& request)
{
   return invocation_response::success("Hello, World!", "application/json");
}

int main()
{
   run_handler(my_handler);
   return 0;
}
```

You can write your custom code in my_handler function. See the main.cpp file

#### 3. Create a file named CMakeLists.txt in the same directory, with the following content:

```
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(hello LANGUAGES CXX)

find_package(aws-lambda-runtime REQUIRED)
add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC AWS::aws-lambda-runtime)
aws_lambda_package_target(${PROJECT_NAME})
```

#### 4. To build this executable, create a build directory and run CMake from there:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/out
make
```

#### 5. To package this executable along with all its dependencies, run the following command:
```bash
make aws-lambda-package-hello
```

This will create a zip file along with all the dependencies. Upload that zip file to AWS Lambda