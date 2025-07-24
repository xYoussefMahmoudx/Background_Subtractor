# Background_Subtractor
This is a project for CSE455 High-Performance Computing Course 

## Project Description
This project implements parallel background subtraction techniques using three different approaches:
1. Sequential implementation (baseline)
2. OpenMP (shared memory parallelism)
3. MPI (distributed memory parallelism)

The goal is to compare the performance of these implementations for removing static backgrounds from video frames to isolate moving objects.

## Implementation Details

### Algorithms
1. **Background Estimation**: Calculates the mean background image by averaging pixel values across multiple frames.
2. **Foreground Mask Generation**: Identifies foreground objects by comparing current frame pixels with the background using a threshold.

### Implementations
- **Sequential**: Baseline single-threaded implementation
- **OpenMP**: Shared memory parallelization using OpenMP directives
- **MPI**: Distributed memory parallelization using MPI for cluster computing

## Requirements

### Software
- C++ compiler (supporting C++11 or later)
- OpenMP (usually included with modern compilers)
- MPI implementation ( OpenMPI)
- .NET Framework (for image handling)
- CMake (optional, for building)
## [Report](https://drive.google.com/file/d/1vMkuKZQ04MdoDcf24SdkAJ4a5Fr9quPm/view?usp=sharing)
