#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <msclr\marshal_cppstd.h>
#include <ctime>
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;


const int NUM_FRAMES = 100;     
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

ColorImage calculateColorBackgroundMean(const vector<ColorImage>& images) {
    ColorImage mean;
    mean.Width = images[0].Width;
    mean.Height = images[0].Height;
    mean.Red = new int[mean.Width * mean.Height];
    mean.Green = new int[mean.Width * mean.Height];
    mean.Blue = new int[mean.Width * mean.Height];

    memset(mean.Red, 0, mean.Width * mean.Height * sizeof(int));
    memset(mean.Green, 0, mean.Width * mean.Height * sizeof(int));
    memset(mean.Blue, 0, mean.Width * mean.Height * sizeof(int));

    for (const auto& img : images) {
        for (int i = 0; i < mean.Height; i++) {
            for (int j = 0; j < mean.Width; j++) {
                mean.Red[i * mean.Width + j] += img.Red[i * mean.Width + j];
                mean.Green[i * mean.Width + j] += img.Green[i * mean.Width + j];
                mean.Blue[i * mean.Width + j] += img.Blue[i * mean.Width + j];
            }
        }
    }

    int numImages = images.size();
    for (int i = 0; i < mean.Height; i++) {
        for (int j = 0; j < mean.Width; j++) {
            mean.Red[i * mean.Width + j] /= numImages;
            mean.Green[i * mean.Width + j] /= numImages;
            mean.Blue[i * mean.Width + j] /= numImages;
        }
    }

    return mean;
}

int* calculateForegroundMask(const ColorImage& background, const ColorImage& currentFrame) {
    int* foregroundMask = new int[background.Width * background.Height];

    for (int i = 0; i < background.Height; i++) {
        for (int j = 0; j < background.Width; j++) {
            int bgGray = (background.Red[i * background.Width + j] +
                background.Green[i * background.Width + j] +
                background.Blue[i * background.Width + j]) / 3;

            int frameGray = (currentFrame.Red[i * background.Width + j] +
                currentFrame.Green[i * background.Width + j] +
                currentFrame.Blue[i * background.Width + j]) / 3;

            int diff = abs(bgGray - frameGray);
            foregroundMask[i * background.Width + j] = (diff > THRESHOLD) ? 255 : 0;
        }
    }

    return foregroundMask;
}

vector<string> getImagePaths() {
    vector<string> paths;
    for (int i = 1; i <= NUM_FRAMES; i++) {
        paths.push_back("..//Data//Input//frame" + to_string(i) + ".png");
    }
    return paths;
}

int main() {
    int start_s, stop_s, TotalTime = 0;
    cout << "Sequential  Background subtractor" << endl;
    vector<string> imagePaths = getImagePaths();
    if (imagePaths.empty()) {
        cout << "No input images found!" << endl;
        return -1;
    }

    vector<ColorImage> colorImages;
    int width, height;
    for (const auto& path : imagePaths) {
        System::String^ imagePath = marshal_as<System::String^>(path);
        ColorImage img = inputColorImage(&width, &height, imagePath);
        colorImages.push_back(img);
    }

    start_s = clock();

    ColorImage colorBackground = calculateColorBackgroundMean(colorImages);

    int* foregroundMask = calculateForegroundMask(colorBackground, colorImages.back());

    stop_s = clock();
    createColorImage(colorBackground, "color_background.png");
    createGrayImage(foregroundMask, width, height, "foreground_mask.png");
    TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;

    for (auto& img : colorImages) {
        delete[] img.Red;
        delete[] img.Green;
        delete[] img.Blue;
    }
    delete[] colorBackground.Red;
    delete[] colorBackground.Green;
    delete[] colorBackground.Blue;
    delete[] foregroundMask;

    cout << "Processing time: " << TotalTime << " ms" << endl;
    cout << "Used parameters:" << endl;
    cout << "  Number of frames: " << NUM_FRAMES << endl;
    cout << "  Threshold value: " << THRESHOLD << endl;

    system("pause");
    return 0;
}