package com.calculator.liuxuncheng.emotiontracker;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.Date;
import java.util.List;

import io.fotoapparat.facedetector.Rectangle;
import io.fotoapparat.preview.Frame;
import io.fotoapparat.preview.FrameProcessor;
import io.fotoapparat.facedetector.processor.FaceDetectorProcessor;


public class FaceTrackProccesor implements FrameProcessor {
    static {
        System.loadLibrary("facetracker_cpp");
    }
    private native void startTrack(byte[] image, int width, int height, int rotation, List<Rectangle> faces, String eye_path);
    private native FaceTrackArgs trackFaces(byte[] image, int width, int height, int rotation, int[] optiflowControl, int[] roiControl);
    private native void reset();
    private static Handler MAIN_THREAD_HANDLER = new Handler(Looper.getMainLooper());
    private int trackTime = 0;
    private List<Rectangle> facesCache;

    private final FaceDetectorProcessor faceDetector;
    private final OnFacesDetectedListener listener;
    private long startTime,nowTime,endTime;
    private String eye_path;
    private EmotionController emotionController;
    private Context context;


    private FaceTrackProccesor(Builder builder) {
        faceDetector = FaceDetectorProcessor.with(builder.context).listener(new FaceDetectorProcessor.OnFacesDetectedListener() {
            @Override
            public void onFacesDetected(List<Rectangle> faces) {
                facesCache=faces;
            }
        }).build();
        listener = builder.listener;
        AssetExtractor assetExtractor = new AssetExtractor(builder.context,"haarcascade_eye_tree_eyeglasses.xml");
        eye_path = assetExtractor.extractIfNeeded().getAbsolutePath();
        context=builder.context;
    }

    public static Builder with(Context context) {
        return new Builder(context);
    }


    @Override
    public void processFrame(Frame frame) {
        int trackStart = 5;
        final FaceTrackArgs faceTrackArgs;
        //final Bitmap bitmap;
        if(frame==null || frame.size==null)return;
        final List<Rectangle> faces;
        try {
            if (trackTime < trackStart) {
                faceDetector.processFrame(frame);
                faces = facesCache;
                if (trackTime == trackStart - 1) {
                    startTrack(frame.image, frame.size.width, frame.size.height, frame.rotation, faces, eye_path);//c++ version
                    emotionController = new EmotionController(faces.size(), context);
                }
                //bitmap = BitmapFactory.decodeByteArray(frame.image,0,frame.image.length);
                faceTrackArgs = new FaceTrackArgs();
                faceTrackArgs.Faces = faces;
                faceTrackArgs.Bitmap = null;
            } else {
                //Frame frame1=new Frame(frame.size, frame.image, frame.rotation);
                if (trackTime == trackStart) endTime = new Date().getTime();
                startTime = new Date().getTime();
                //Log.d("TIME of Camera", -(endTime - startTime) + "ms");
                faceTrackArgs = trackFaces(frame.image, frame.size.width, frame.size.height, frame.rotation,
                        emotionController.optiControl, emotionController.roiContral);//c++ version
                //Log.d("FPS", 1000/(double)(new Date().getTime()-nowTime)+"fps");
                nowTime = new Date().getTime();
                //Log.d("TIME of TRACKER", (nowTime - startTime) + "ms");
                faceTrackArgs.Time = nowTime - startTime;
                emotionController.LifeUpdate(faceTrackArgs);
                faceTrackArgs.emotion = emotionController.GetEmotion();
                faceTrackArgs.offloadTime = emotionController.inTime;
                endTime = new Date().getTime();
                //Log.d("TIME of DECIDE", (endTime - nowTime) + "ms");
            }
            if(faceTrackArgs.Faces!=null && faceTrackArgs.Faces.size()>0) {
                trackTime++;
            }
            else
            {
                trackTime=0;
            }
            MAIN_THREAD_HANDLER.post(new Runnable() {
                @Override
                public void run() {
                    listener.onFacesDetected(faceTrackArgs);
                }
            });
        }
        catch (Exception e){
            reset();
            emotionController.Reset();
        }
    }

    /**
     * Notified when faces are detected.
     */
    public interface OnFacesDetectedListener {

        /**
         * Null-object for {@link OnFacesDetectedListener}.
         */
        OnFacesDetectedListener NULL = new OnFacesDetectedListener() {
            @Override
            public void onFacesDetected(FaceTrackArgs faceTrackArgs) {
                // Do nothing
            }
        };

        /**
         * Called when faces are detected. Always called on the main thread.
         *
         * @param faceTrackArgs detected faces. If no faces were detected - an empty list.
         */
        void onFacesDetected(FaceTrackArgs faceTrackArgs);

    }

    public void Reset(){
        trackTime = 0;
        reset();//c++ version
    }

    /**
     * Builder for {@link FaceTrackProccesor}.
     */
    public static class Builder {

        private final Context context;
        private OnFacesDetectedListener listener = OnFacesDetectedListener.NULL;

        private Builder(Context context) {
            this.context = context;
        }

        /**
         * @param listener which will be notified when faces are detected.
         */
        public Builder listener(OnFacesDetectedListener listener) {
            this.listener = listener != null
                    ? listener
                    : OnFacesDetectedListener.NULL;

            return this;
        }

        public FaceTrackProccesor build() {
            return new FaceTrackProccesor(this);
        }

    }
}
