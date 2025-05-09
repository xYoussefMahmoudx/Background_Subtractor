#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <msclr\marshal_cppstd.h>
#include <ctime>
#include <mpi.h>

#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

const int NUM_FRAMES = 20;
const int THRESHOLD = 30;

struct ColorImage {
    int* Red;
    int* Green;
    int* Blue;
    int Width;
    int Height;
};

ColorImage inputColorImage(int* w, int* h, System::String^ imagePath) {
    ColorImage img;
    System::Drawing::Bitmap BM(imagePath);
    img.Width = BM.Width;
    img.Height = BM.Height;
    *w = BM.Width;
    *h = BM.Height;

    img.Red = new int[BM.Height * BM.Width];
    img.Green = new int[BM.Height * BM.Width];
    img.Blue = new int[BM.Height * BM.Width];

    for (int i = 0; i < BM.Height; i++) {
        for (int j = 0; j < BM.Width; j++) {
            System::Drawing::Color c = BM.GetPixel(j, i);
            img.Red[i * BM.Width + j] = c.R;
            img.Green[i * BM.Width + j] = c.G;
            img.Blue[i * BM.Width + j] = c.B;
        }
    }
    return img;
}

void createColorImage(ColorImage img, string filename) {
    System::Drawing::Bitmap MyNewImage(img.Width, img.Height);

    for (int i = 0; i < MyNewImage.Height; i++) {
        for (int j = 0; j < MyNewImage.Width; j++) {
            int r = max(0, min(255, img.Red[i * img.Width + j]));
            int g = max(0, min(255, img.Green[i * img.Width + j]));
            int b = max(0, min(255, img.Blue[i * img.Width + j]));

            System::Drawing::Color c = System::Drawing::Color::FromArgb(r, g, b);
            MyNewImage.SetPixel(j, i, c);
        }
    }
    MyNewImage.Save("..//Data//Output//" + gcnew System::String(filename.c_str()));
    cout << "Color image saved: " << filename << endl;
}

void createGrayImage(int* image, int width, int height, string filename) {
    System::Drawing::Bitmap MyNewImage(width, height);

    for (int i = 0; i < MyNewImage.Height; i++) {
        for (int j = 0; j < MyNewImage.Width; j++) {
            int val = max(0, min(255, image[i * width + j]));
            System::Drawing::Color c = System::Drawing::Color::FromArgb(val, val, val);
            MyNewImage.SetPixel(j, i, c);
        }
    }
    MyNewImage.Save("..//Data//Output//" + gcnew System::String(filename.c_str()));
    cout << "Grayscale image saved: " << filename << endl;
}

vector<string> getImagePaths() {
    vector<string> paths;
    for (int i = 1; i <= NUM_FRAMES; i++) {
        paths.push_back("..//Data//Input//frame" + to_string(i) + ".png");
    }
    return paths;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    vector<string> imagePaths;
    vector<ColorImage> colorImages;
    int width = 0, height = 0;
    int start_s, stop_s, TotalTime = 0;

    // Rank 0 loads all images
    if (rank == 0) {
        cout << "Parallel Background subtractor using MPI" << endl;
        imagePaths = getImagePaths();
        for (const auto& path : imagePaths) {
            System::String^ imagePath = marshal_as<System::String^>(path);
            ColorImage img = inputColorImage(&width, &height, imagePath);
            colorImages.push_back(img);
        }

        cout << "Images loaded by rank 0" << endl;
    }

    // Broadcast image dimensions to all processes
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int totalPixels = width * height;
    int pixelsPerProcess = totalPixels / size;
    int remainder = totalPixels % size;


    vector<int> counts(size, pixelsPerProcess);
    vector<int> displs(size, 0);

    for (int i = 0; i < remainder; i++) {
        counts[i]++;
    }

    for (int i = 1; i < size; i++) {
        displs[i] = displs[i - 1] + counts[i - 1];
    }

    int myCount = counts[rank];
    int myDispl = displs[rank];

 
    int* localRedSum = new int[myCount]();
    int* localGreenSum = new int[myCount]();
    int* localBlueSum = new int[myCount]();

    
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        start_s = clock();
    }

    // Scatter image data and calculate local sums
    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        int* redChannel = nullptr;
        int* greenChannel = nullptr;
        int* blueChannel = nullptr;

        if (rank == 0) {
            redChannel = colorImages[frame].Red;
            greenChannel = colorImages[frame].Green;
            blueChannel = colorImages[frame].Blue;
        }

        int* localRed = new int[myCount];
        int* localGreen = new int[myCount];
        int* localBlue = new int[myCount];

        // Scatter each channel
        MPI_Scatterv(redChannel, counts.data(), displs.data(), MPI_INT,
            localRed, myCount, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Scatterv(greenChannel, counts.data(), displs.data(), MPI_INT,
            localGreen, myCount, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Scatterv(blueChannel, counts.data(), displs.data(), MPI_INT,
            localBlue, myCount, MPI_INT, 0, MPI_COMM_WORLD);

        // Accumulate sums
        for (int i = 0; i < myCount; i++) {
            localRedSum[i] += localRed[i];
            localGreenSum[i] += localGreen[i];
            localBlueSum[i] += localBlue[i];
        }

        delete[] localRed;
        delete[] localGreen;
        delete[] localBlue;
    }

    // Calculate local mean
    for (int i = 0; i < myCount; i++) {
        localRedSum[i] /= NUM_FRAMES;
        localGreenSum[i] /= NUM_FRAMES;
        localBlueSum[i] /= NUM_FRAMES;
    }

    // Gather background results to rank 0
    int* backgroundRed = nullptr;
    int* backgroundGreen = nullptr;
    int* backgroundBlue = nullptr;

    if (rank == 0) {
        backgroundRed = new int[totalPixels];
        backgroundGreen = new int[totalPixels];
        backgroundBlue = new int[totalPixels];
    }

    MPI_Gatherv(localRedSum, myCount, MPI_INT,
        backgroundRed, counts.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gatherv(localGreenSum, myCount, MPI_INT,
        backgroundGreen, counts.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gatherv(localBlueSum, myCount, MPI_INT,
        backgroundBlue, counts.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);


    int* lastFrameRed = nullptr;
    int* lastFrameGreen = nullptr;
    int* lastFrameBlue = nullptr;

    if (rank == 0) {
        lastFrameRed = colorImages.back().Red;
        lastFrameGreen = colorImages.back().Green;
        lastFrameBlue = colorImages.back().Blue;
    }


    int* localBgRed = new int[myCount];
    int* localBgGreen = new int[myCount];
    int* localBgBlue = new int[myCount];
    int* localFrameRed = new int[myCount];
    int* localFrameGreen = new int[myCount];
    int* localFrameBlue = new int[myCount];

    MPI_Scatterv(backgroundRed, counts.data(), displs.data(), MPI_INT,
        localBgRed, myCount, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(backgroundGreen, counts.data(), displs.data(), MPI_INT,
        localBgGreen, myCount, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(backgroundBlue, counts.data(), displs.data(), MPI_INT,
        localBgBlue, myCount, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Scatterv(lastFrameRed, counts.data(), displs.data(), MPI_INT,
        localFrameRed, myCount, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(lastFrameGreen, counts.data(), displs.data(), MPI_INT,
        localFrameGreen, myCount, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(lastFrameBlue, counts.data(), displs.data(), MPI_INT,
        localFrameBlue, myCount, MPI_INT, 0, MPI_COMM_WORLD);


    int* localForeground = new int[myCount];
    for (int i = 0; i < myCount; i++) {
        int bgGray = (localBgRed[i] + localBgGreen[i] + localBgBlue[i]) / 3;
        int frameGray = (localFrameRed[i] + localFrameGreen[i] + localFrameBlue[i]) / 3;
        int diff = abs(bgGray - frameGray);
        localForeground[i] = (diff > THRESHOLD) ? 255 : 0;
    }


    int* foregroundMask = nullptr;
    if (rank == 0) {
        foregroundMask = new int[totalPixels];
    }

    MPI_Gatherv(localForeground, myCount, MPI_INT,
        foregroundMask, counts.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);

 
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        stop_s = clock();
        TotalTime = (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;

        // Create background image structure
        ColorImage colorBackground;
        colorBackground.Width = width;
        colorBackground.Height = height;
        colorBackground.Red = backgroundRed;
        colorBackground.Green = backgroundGreen;
        colorBackground.Blue = backgroundBlue;

        // Save results
        createColorImage(colorBackground, "color_background_parallel.png");
        createGrayImage(foregroundMask, width, height, "foreground_mask_parallel.png");

        cout << "Processing time: " << TotalTime << " ms" << endl;
        cout << "Used parameters:" << endl;
        cout << "  Number of frames: " << NUM_FRAMES << endl;
        cout << "  Threshold value: " << THRESHOLD << endl;
        cout << "  Number of MPI processes: " << size << endl;

        // Clean up
        delete[] backgroundRed;
        delete[] backgroundGreen;
        delete[] backgroundBlue;
        delete[] foregroundMask;

        for (auto& img : colorImages) {
            delete[] img.Red;
            delete[] img.Green;
            delete[] img.Blue;
        }
    }

    // Clean up local memory
    delete[] localRedSum;
    delete[] localGreenSum;
    delete[] localBlueSum;
    delete[] localBgRed;
    delete[] localBgGreen;
    delete[] localBgBlue;
    delete[] localFrameRed;
    delete[] localFrameGreen;
    delete[] localFrameBlue;
    delete[] localForeground;

    MPI_Finalize();

 
    return 0;
}