#include <iostream>
#include <vector>
#include <omp.h>
#include <cmath>
#include <algorithm>
#include <msclr\marshal_cppstd.h>
#include <ctime>

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

const int NUM_FRAMES = 20;          
const int DEFAULT_THREADS = 4;      
const int DEFAULT_THRESHOLD = 30;

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


ColorImage calculateColorBackgroundMean(const vector<ColorImage>& images, int num_threads = DEFAULT_THREADS) {
    ColorImage mean;
    mean.Width = images[0].Width;
    mean.Height = images[0].Height;

    mean.Red = new int[mean.Width * mean.Height]();
    mean.Green = new int[mean.Width * mean.Height]();
    mean.Blue = new int[mean.Width * mean.Height]();

    omp_set_num_threads(num_threads);
#pragma omp parallel for collapse(2) schedule(dynamic)
    for (int i = 0; i < mean.Height; i++) {
        for (int j = 0; j < mean.Width; j++) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (const auto& img : images) {
                sum_r += img.Red[i * mean.Width + j];
                sum_g += img.Green[i * mean.Width + j];
                sum_b += img.Blue[i * mean.Width + j];
            }

            mean.Red[i * mean.Width + j] = sum_r / images.size();
            mean.Green[i * mean.Width + j] = sum_g / images.size();
            mean.Blue[i * mean.Width + j] = sum_b / images.size();
        }
    }
    return mean;
}

int* calculateForegroundMask(const ColorImage& background,
    const ColorImage& currentFrame,
    int threshold = DEFAULT_THRESHOLD,
    int num_threads = DEFAULT_THREADS) {
    int* mask = new int[background.Width * background.Height];

    omp_set_num_threads(num_threads);
#pragma omp parallel for collapse(2) schedule(dynamic)
    for (int i = 0; i < background.Height; i++) {
        for (int j = 0; j < background.Width; j++) {
            int bg_gray = (background.Red[i * background.Width + j] +
                background.Green[i * background.Width + j] +
                background.Blue[i * background.Width + j]) / 3;

            int frame_gray = (currentFrame.Red[i * background.Width + j] +
                currentFrame.Green[i * background.Width + j] +
                currentFrame.Blue[i * background.Width + j]) / 3;

            mask[i * background.Width + j] = (abs(bg_gray - frame_gray) > threshold) ? 255 : 0;
        }
    }
    return mask;
}


vector<string> getImagePaths(int num_frames) {
    vector<string> paths;
    for (int i = 1; i <= num_frames; i++) {
        paths.push_back("..//Data//Input//frame" + to_string(i) + ".png");
    }
    return paths;
}

int main() {
    int start_s, stop_s, TotalTime = 0;
    cout << "OpenMP Background subtractor" << endl;
    // Load images
    vector<ColorImage> frames;
    auto paths = getImagePaths(NUM_FRAMES);
    int width, height;

    for (const auto& path : paths) {
        System::String^ imgPath = marshal_as<System::String^>(path);
        frames.push_back(inputColorImage(&width, &height, imgPath));
    }

    start_s = clock();

    ColorImage bg = calculateColorBackgroundMean(frames, DEFAULT_THREADS);
    int* mask = calculateForegroundMask(bg, frames.back(), DEFAULT_THRESHOLD, DEFAULT_THREADS);
    

    stop_s = clock();
    createColorImage(bg, "background.png");
    createGrayImage(mask, width, height, "mask.png");
    TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;

    delete[] mask;
    for (auto& frame : frames) {
        delete[] frame.Red;
        delete[] frame.Green;
        delete[] frame.Blue;
    }
    delete[] bg.Red; delete[] bg.Green; delete[] bg.Blue;

   
    cout << "Processing time: " << TotalTime << " ms" << endl;
    cout << "Used parameters: " << endl;
    cout << "  Number of frames: " << NUM_FRAMES << endl;
    cout << "  Number of threads: " << DEFAULT_THREADS << endl;
    cout << "  Threshold value: " << DEFAULT_THRESHOLD << endl;

    system("pause");
    return 0;
}