package com.calculator.liuxuncheng.emotiontracker;

import android.graphics.Bitmap;

import java.util.List;

import io.fotoapparat.facedetector.Rectangle;

public class FaceTrackArgs {
    List<Rectangle> Faces;
    Bitmap Bitmap;
    long Time;
    byte[][] ROIdatas;
    int[] ROIstatuses;
    float[][] Optiflows;
    int[] Optistatuses;
    int isTrackFace = 0;
    int isTrackEye = 0;
    int isOffloading = 0;
    String emotion;
    long offloadTime=0;

    public FaceTrackArgs(List<Rectangle> faces, Bitmap bitmap){
        Faces=faces;
        Bitmap=bitmap;
    }
    public FaceTrackArgs(){
    }
}
