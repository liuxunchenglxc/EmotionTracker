package com.calculator.liuxuncheng.emotiontracker;

import android.content.Context;
import android.support.v4.content.res.TypedArrayUtils;
import android.util.Log;
import android.util.SparseArray;

import com.megvii.facepp.api.FacePPApi;
import com.megvii.facepp.api.IFacePPCallBack;
import com.megvii.facepp.api.bean.DetectResponse;
import com.megvii.facepp.api.bean.Emotion;
import org.tensorflow.lite.Interpreter;
import org.tensorflow.lite.gpu.GpuDelegate;

import java.io.File;
import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

class EmotionController {
    int[] roiContral,optiControl,emotionE;
    private FacePPApi facePPApi = new FacePPApi("bv8Iw6Ss4UdmVGx1ZeeIY4C1sy2SaD7j","mwB3NEI0g9yqOy_LqEx173V49slbCXfj");
    private Map<String,String> params = new HashMap<>();
    private float[][] emotionBase;
    private float[][] emotionResidual;
    private boolean isCalcOptiflow = false;
    private Interpreter interpreter;
    private Interpreter interpreter_r;
    private Interpreter interpreter_d;
    private GpuDelegate gpuDelegate;
    private int isStartTrackEye = 0;
    private int isStartOffloading = 0;
    private long sTime;
    long inTime=0;
    EmotionController(int size, Context context){
        roiContral = new int[size];
        optiControl = new int[size];
        emotionE = new int[size];
        for(int i = 0 ; i<size ; i++){
            roiContral[i] = 0;
            optiControl[i] = 0;
            emotionE[i] = 0;
        }
        params.put("return_attributes", "emotion");
        emotionBase = new float[size][7];
        emotionResidual = new float[size][7];
        gpuDelegate = new GpuDelegate();
        Interpreter.Options options = (new Interpreter.Options()).addDelegate(gpuDelegate);
        AssetExtractor assetExtractor = new AssetExtractor(context,"model3_200_200_50-0-2-0.tflite");
        File tffile = assetExtractor.extractIfNeeded();
        interpreter = new Interpreter(tffile);//, options);
        assetExtractor = new AssetExtractor(context,"model3_200_200_50-0-2-0-r.tflite");
        tffile = assetExtractor.extractIfNeeded();
        interpreter_r = new Interpreter(tffile);
        assetExtractor = new AssetExtractor(context,"model_d_0.tflite");
        tffile = assetExtractor.extractIfNeeded();
        interpreter_d = new Interpreter(tffile);
        sTime = new Date().getTime();
    }
    @Override
    protected void finalize() throws Throwable {
        gpuDelegate.close();
        super.finalize();
    }

    private void EmotionOK(int i){
        optiControl[i]=1;
        emotionE[i]=1;
        isCalcOptiflow = true;
        for(int ii = 0; ii<7; ii++){
            emotionResidual[i][ii]=0;
        }
        inTime = (new Date().getTime()) - sTime;
        //Log.d("offloading time","" + inTime + "ms");
    }
    private void EmotionWait(int i){
        optiControl[i]=0;
        roiContral[i]=0;
        isCalcOptiflow = false;
        long lTime = inTime+sTime;
        sTime = new Date().getTime();
        //Log.d("insist time","" + (sTime-lTime) + "ms");
    }
    void Reset(){
        for(int i = 0 ; i<roiContral.length ; i++){
            roiContral[i] = 0;
            optiControl[i] = 0;
        }
    }
    String GetEmotion(){
        String s = "";
        for(int i = 0; i<emotionResidual.length-1; i++){
            if(emotionE[i]==0){
                s = s.concat("Face"+i+": waiting for face expression recognition\n");
                continue;
            }
            int index = maxIndex(emotionResidual[i]);
            switch (index){
                case 0:
                    s = s.concat("Face"+i+": happiness\n");
                    break;
                case 1:
                    s = s.concat("Face"+i+": anger\n");
                    break;
                case 2:
                    s = s.concat("Face"+i+": disgust\n");
                    break;
                case 3:
                    s = s.concat("Face"+i+": fear\n");
                    break;
                case 4:
                    s = s.concat("Face"+i+": neutral\n");
                    break;
                case 5:
                    s = s.concat("Face"+i+": sadness\n");
                    break;
                case 6:
                    s = s.concat("Face"+i+": surprise\n");
                    break;
            }
        }
        return s;
    }
    private int maxIndex(float[] emotion){
        float max=0;
        int index=0;
        for(int i= 0; i<7; i++){
            if(emotion[i]>max){
                max=emotion[i];
                index=i;
            }
        }
        return index;
    }
    private void EmotionUpdate(float[] emotions, int i){
        emotionBase[i] = emotions;
    }
    private void OptiflowUpdate(float[] flow, int i){
        long stime = new Date().getTime();
        Object input = new float[1][49];
        System.arraycopy(flow,0,((float[][])input)[0],0,42);
        System.arraycopy(emotionBase[i],0,((float[][])input)[0],42,7);
        Object output = new float[1][7];
        interpreter.run(input,output);
        //Log.d("Track Predi", new Date().getTime()-stime+"");

        Object react = new float[1][7];
        for(int ii = 0; ii<7; ii++){
            ((float[][])react)[0][ii]=((float[][])output)[0][ii]-emotionBase[i][ii];
        }
        Object residual = new float[1][7];
        interpreter_r.run(react,residual);
        boolean roiDO = false;
        for(int ii = 0; ii<7; ii++){
            emotionResidual[i][ii]+=((float[][])residual)[0][ii]>0?((float[][])residual)[0][ii]:0;
        }
        Object decide = new float[1][1];
        Object aresidual = new float[1][7];
        ((float[][])aresidual)[0] = emotionResidual[i];
        interpreter_d.run(aresidual, decide);

        float d = ((float[][])decide)[0][0];
        Log.d("Track D", d+"");
        float t0 = sTime + inTime;
        float t = new Date().getTime();
        if(d>0.7) roiDO=true;
        if(roiDO)roiContral[i]=1;

        emotionBase[i]=((float[][])output)[0];
    }
    void LifeUpdate(FaceTrackArgs faceTrackArgs){
        faceTrackArgs.isTrackFace = 1;
        for (int i = 0; i < faceTrackArgs.ROIstatuses.length; i++) {
            final int k = i;
            switch (faceTrackArgs.ROIstatuses[i]){
                case 1:
                    isStartTrackEye = 1;
                    isStartOffloading = 1;
                    //do now
                    byte[] roi_png = faceTrackArgs.ROIdatas[i];
                    //Log.d("png size","" + roi_png.length);
                    EmotionWait(i);
                    //do future
                    facePPApi.detect(params, roi_png, new IFacePPCallBack<DetectResponse>() {
                        @Override
                        public void onSuccess(DetectResponse detectResponse) {
                            Emotion emotion = detectResponse.getFaces().get(0).getAttributes().getEmotion();
                            float[] emotions = new float[7];
                            emotions[0] = emotion.getHappiness();
                            emotions[1] = emotion.getAnger();
                            emotions[2] = emotion.getDisgust();
                            emotions[3] = emotion.getFear();
                            emotions[4] = emotion.getNeutral();
                            emotions[5] = emotion.getSadness();
                            emotions[6] = emotion.getSurprise();
                            EmotionUpdate(emotions, k);
                            EmotionOK(k);
                            roiContral[k] = 0;
                            isStartOffloading = 0;
                        }

                        @Override
                        public void onFailed(String s) {
                            roiContral[k] = 1;
                            isStartOffloading = 0;
                        }
                    });
                    break;
                case 0:
                    break;
            }
            switch (faceTrackArgs.Optistatuses[i]){
                case 1:
                    if(!isCalcOptiflow)break;
                    OptiflowUpdate(faceTrackArgs.Optiflows[i], k);
                    break;
                case 0:
                    break;
            }
        }
        faceTrackArgs.isTrackEye = isStartTrackEye;
        faceTrackArgs.isOffloading = isStartOffloading;
    }
}
