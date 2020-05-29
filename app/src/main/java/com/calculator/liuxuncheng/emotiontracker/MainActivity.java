package com.calculator.liuxuncheng.emotiontracker;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.TextView;


import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import io.fotoapparat.Fotoapparat;
import io.fotoapparat.FotoapparatSwitcher;
import io.fotoapparat.facedetector.Rectangle;
import io.fotoapparat.facedetector.view.RectanglesView;
import io.fotoapparat.parameter.LensPosition;
import io.fotoapparat.parameter.Size;
import io.fotoapparat.parameter.range.Range;
import io.fotoapparat.parameter.selector.SelectorFunction;
import io.fotoapparat.view.CameraView;

import static io.fotoapparat.log.Loggers.fileLogger;
import static io.fotoapparat.log.Loggers.logcat;
import static io.fotoapparat.log.Loggers.loggers;
import static io.fotoapparat.parameter.selector.LensPositionSelectors.lensPosition;

public class MainActivity extends AppCompatActivity {

    private final PermissionsDelegate permissionsDelegate = new PermissionsDelegate(this);
    private boolean hasCameraPermission;
    private CameraView cameraView;
    private RectanglesView rectanglesView;
    private TextView timeTextView;
    private TextView faceTextView;
    private TextView eyeTextView;
    private TextView offloadTextView;
    private TextView emotionTextView;
    //private ImageView imageView;

    private FotoapparatSwitcher fotoapparatSwitcher;
    private Fotoapparat frontFotoapparat;
    private Fotoapparat backFotoapparat;
    private FaceTrackProccesor frontFaceTracker;
    private FaceTrackProccesor backFaceTracker;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        cameraView = (CameraView) findViewById(R.id.camera_view);
        rectanglesView = (RectanglesView) findViewById(R.id.rectanglesView);
        timeTextView = (TextView) findViewById(R.id.timetextView);
        faceTextView = (TextView) findViewById(R.id.facetextView);
        eyeTextView = (TextView) findViewById(R.id.eyetextView);
        offloadTextView = (TextView) findViewById(R.id.offloadtextView);
        emotionTextView = (TextView) findViewById(R.id.emotiontextView);
        //imageView = (ImageView) findViewById(R.id.imageView);
        hasCameraPermission = permissionsDelegate.hasCameraPermission();

        if (hasCameraPermission) {
            cameraView.setVisibility(View.VISIBLE);
        } else {
            permissionsDelegate.requestCameraPermission();
        }
        //if (permissionsDelegate.hasInternetPermission()) {
        //} else {
        //    permissionsDelegate.requestInternetPermission();
        //}

        frontFaceTracker =createFaceTracker("FrontFT #");
        backFaceTracker =createFaceTracker("BackFT  #");
        frontFotoapparat = createFotoapparat(LensPosition.FRONT, frontFaceTracker);
        backFotoapparat = createFotoapparat(LensPosition.BACK, backFaceTracker);
        fotoapparatSwitcher = FotoapparatSwitcher.withDefault(backFotoapparat);

        View switchCameraButton = findViewById(R.id.switchCamera);
        switchCameraButton.setVisibility(
                canSwitchCameras()
                        ? View.VISIBLE
                        : View.GONE
        );
        switchCameraButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                switchCamera();
            }
        });
    }
    private boolean canSwitchCameras() {
        return frontFotoapparat.isAvailable() == backFotoapparat.isAvailable();
    }

    private Fotoapparat createFotoapparat(LensPosition position, FaceTrackProccesor faceTrackProccesor) {
        return Fotoapparat
                .with(this)
                .into(cameraView)
                .lensPosition(lensPosition(position))
                .frameProcessor(faceTrackProccesor)
                .logger(loggers(
                        logcat(),
                        fileLogger(this)
                ))
                .previewSize(new SelectorFunction<Collection<Size>, Size>() {
                    @Override
                    public Size select(Collection<Size> sizes) {
                        Size r = new Size(1000000,1000000);
                        for(Size s:sizes){
                            if(s.width<r.width)r=s;
                        }
                        return r;
                    }
                })
                .build();
    }
    private FaceTrackProccesor createFaceTracker(String tag){
        return FaceTrackProccesor.with(this)
                .listener(new FaceTrackProccesor.OnFacesDetectedListener() {
                    @Override
                    public void onFacesDetected(FaceTrackArgs faceTrackArgs) {
                        //Log.d("&&&", "Detected faces: " + faces.size());
                        if(faceTrackArgs.Faces!=null && faceTrackArgs.Faces.size()>0) {
                            List<Rectangle> facess=new ArrayList<>();
                            if(fotoapparatSwitcher.getCurrentFotoapparat()==frontFotoapparat){
                                for (Rectangle face:faceTrackArgs.Faces
                                     ) {
                                    facess.add(new Rectangle(1-face.getWidth()-face.getX(),face.getY(),face.getWidth(),face.getHeight()));
                                }
                            }
                            else{
                                facess=faceTrackArgs.Faces;
                            }
                            rectanglesView.setRectangles(facess);
                            timeTextView.setText("Process Time："+faceTrackArgs.Time+"ms");
                            faceTextView.setText("Face Track："+((faceTrackArgs.isTrackFace==1)?"start":"wait for stably face detecting"));
                            eyeTextView.setText("Eye Detect："+((faceTrackArgs.isTrackEye==1)?"finish":((faceTrackArgs.isTrackFace==1)?"wait for eye detecting":"wait for face tracking")));
                            offloadTextView.setText("Task Offloading："+((faceTrackArgs.isOffloading==1)?"offloading":((faceTrackArgs.offloadTime==0)?"idle":"time delay "+faceTrackArgs.offloadTime+"ms")));
                            emotionTextView.setText("Face Expression：\n"+faceTrackArgs.emotion);
                            //Log.d(tag,"rect is "+faces.get(0).getX()*100+","+faces.get(0).getY()*100+" "+
                            //        faces.get(0).getWidth()*100+"*"+faces.get(0).getHeight()*100);
                            //if(bitmap!=null){
                                //imageView.setImageBitmap(bitmap);
                                //imageView.setVisibility(View.VISIBLE);
                                //imageView.bringToFront();
                                //rectanglesView.bringToFront();
                            //}
                            //else {
                                //cameraView.bringToFront();
                                //rectanglesView.bringToFront();
                            //}
                        }
                    }
                })
                .build();
    }

    private void switchCamera() {
        if (fotoapparatSwitcher.getCurrentFotoapparat() == frontFotoapparat) {
            fotoapparatSwitcher.switchTo(backFotoapparat);
            frontFaceTracker.Reset();
        } else {
            fotoapparatSwitcher.switchTo(frontFotoapparat);
            backFaceTracker.Reset();
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        if (hasCameraPermission) {
            fotoapparatSwitcher.start();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (hasCameraPermission) {
            fotoapparatSwitcher.stop();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (permissionsDelegate.resultGranted(requestCode, permissions, grantResults)) {
            fotoapparatSwitcher.start();
            cameraView.setVisibility(View.VISIBLE);
        }
    }

}
