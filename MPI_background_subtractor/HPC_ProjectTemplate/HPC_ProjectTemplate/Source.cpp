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

    int start_s, stop_s, TotalTime = 0;
    int width = 0, height = 0;

    vector<ColorImage> colorImages;
    ColorImage background;
    ColorImage currentFrame;

    if (rank == 0) {
        cout << "MPI Parallel Background Subtractor" << endl;
        vector<string> imagePaths = getImagePaths();
        for (const auto& path : imagePaths) {
            System::String^ imagePath = marshal_as<System::String^>(path);
            //cout << "Rank " << rank << " loading image: " << path << endl;
            ColorImage img = inputColorImage(&width, &height, imagePath);
            colorImages.push_back(img);
        }
    }

    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int totalPixels = width * height;

    int chunkSize = totalPixels / size;
    int remainder = totalPixels % size;
    int* counts = new int[size];
    int* displs = new int[size];
    for (int i = 0; i < size; ++i) {
        counts[i] = chunkSize + (i < remainder ? 1 : 0);
        displs[i] = (i == 0) ? 0 : displs[i - 1] + counts[i - 1];
    }

    int* partialRed = new int[totalPixels] {};
    int* partialGreen = new int[totalPixels] {};
    int* partialBlue = new int[totalPixels] {};

    if (rank == 0) {
        for (const auto& img : colorImages) {
            for (int i = 0; i < totalPixels; ++i) {
                partialRed[i] += img.Red[i];
                partialGreen[i] += img.Green[i];
                partialBlue[i] += img.Blue[i];
            }
        }
    }

    int* localRed = new int[totalPixels] {};
    int* localGreen = new int[totalPixels] {};
    int* localBlue = new int[totalPixels] {};
    if (rank == 0) {
        start_s = clock();
    }

    MPI_Reduce(partialRed, localRed, totalPixels, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(partialGreen, localGreen, totalPixels, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(partialBlue, localBlue, totalPixels, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    int* bgRed = nullptr;
    int* bgGreen = nullptr;
    int* bgBlue = nullptr;

    if (rank == 0) {
        int numImages = colorImages.size();
        bgRed = new int[totalPixels];
        bgGreen = new int[totalPixels];
        bgBlue = new int[totalPixels];

        for (int i = 0; i < totalPixels; ++i) {
            bgRed[i] = localRed[i] / numImages;
            bgGreen[i] = localGreen[i] / numImages;
            bgBlue[i] = localBlue[i] / numImages;
        }

        background = { bgRed, bgGreen, bgBlue, width, height };
        currentFrame = colorImages.back();
    }



    int* bgR = new int[totalPixels];
    int* bgG = new int[totalPixels];
    int* bgB = new int[totalPixels];
    int* frameR = new int[totalPixels];
    int* frameG = new int[totalPixels];
    int* frameB = new int[totalPixels];

    if (rank == 0) {
        memcpy(bgR, bgRed, totalPixels * sizeof(int));
        memcpy(bgG, bgGreen, totalPixels * sizeof(int));
        memcpy(bgB, bgBlue, totalPixels * sizeof(int));
        memcpy(frameR, currentFrame.Red, totalPixels * sizeof(int));
        memcpy(frameG, currentFrame.Green, totalPixels * sizeof(int));
        memcpy(frameB, currentFrame.Blue, totalPixels * sizeof(int));
    }

    MPI_Bcast(bgR, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(bgG, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(bgB, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(frameR, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(frameG, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(frameB, totalPixels, MPI_INT, 0, MPI_COMM_WORLD);

    int localSize = counts[rank];
    int localStart = displs[rank];
    int* localMask = new int[localSize];

    for (int i = 0; i < localSize; ++i) {
        int idx = localStart + i;
        int bgGray = (bgR[idx] + bgG[idx] + bgB[idx]) / 3;
        int frameGray = (frameR[idx] + frameG[idx] + frameB[idx]) / 3;
        int diff = abs(bgGray - frameGray);
        localMask[i] = (diff > THRESHOLD) ? 255 : 0;
    }

    int* fullMask = nullptr;
    if (rank == 0) fullMask = new int[totalPixels];

    MPI_Gatherv(localMask, localSize, MPI_INT, fullMask, counts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        stop_s = clock();
        createColorImage(background, "color_background.png");
        createGrayImage(fullMask, width, height, "foreground_mask.png");
        TotalTime = (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;

        for (auto& img : colorImages) {
            delete[] img.Red;
            delete[] img.Green;
            delete[] img.Blue;
        }
        delete[] fullMask;
        delete[] bgRed;
        delete[] bgGreen;
        delete[] bgBlue;

        cout << "Processing time: " << TotalTime << " ms" << endl;
        cout << "Used parameters:" << endl;
        cout << "  Number of frames: " << NUM_FRAMES << endl;
        cout << "  Threshold value: " << THRESHOLD << endl;
    }

    delete[] localRed;
    delete[] localGreen;
    delete[] localBlue;
    delete[] partialRed;
    delete[] partialGreen;
    delete[] partialBlue;
    delete[] bgR;
    delete[] bgG;
    delete[] bgB;
    delete[] frameR;
    delete[] frameG;
    delete[] frameB;
    delete[] localMask;
    delete[] counts;
    delete[] displs;

    MPI_Finalize();
    return 0;
}
