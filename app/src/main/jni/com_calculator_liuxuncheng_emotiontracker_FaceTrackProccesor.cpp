//
// Created by Mechrevo on 2019/8/13.
//

#include <vector>
#include <opencv2/opencv.hpp>

#include <Android/log.h>
#include <Android/asset_manager.h>
#include <Android/asset_manager_jni.h>
#include <DataKeeper.h>

#define TAG "com.calculator.liuxuncheng.jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__)

#include "com_calculator_liuxuncheng_emotiontracker_FaceTrackProccesor.h"

using namespace cv;

//save last image for next track, but for debug before
Mat oldImage;
//arguments for hist and back project
int channels[] = {1,2};
float range1[] = {133,173};
float range2[] = {77,127};
const float *ranges[] = {range1,range2};
int histSize[] = {64,64};
//store hist and provide uniform operations
DataKeeper<Mat> histKeeper;
//store eye positions
DataKeeper<Point2f> leyeKeeper,reyeKeeper;
DataKeeper<int> eyeStatusKeeper;
//save last rect for
std::vector<Rect> lastRects;
//save all stable rect history
DataKeeper<std::vector<Rect>> stableSetKeeper;
//arguments for tracking, size and status
int fullWidth;
int fullHeight;
int hasStarted = 0;
//eye detector
CascadeClassifier eyeClassifier;
//jni
jclass cls_rtg;
jmethodID rtg_x;
jmethodID rtg_y;
jmethodID rtg_w;
jmethodID rtg_h;
jmethodID rtg_init;
jclass cls_arraylist;
jmethodID aylst_init;
jmethodID aylst_add;
jclass r_class;
jmethodID init_r;
jfieldID r_Faces;
jmethodID arraylist_get;
jmethodID arraylist_size;
jfieldID r_ROIstatuses;
jfieldID r_Optistatuses;
jfieldID r_ROIdatas;
jfieldID r_Optiflows;

//for cell phone can rotate
void rotateImage(Mat &image, int32_t rotationDegrees) {
    switch (rotationDegrees) {
        case 90:
            rotate(image, image, ROTATE_90_COUNTERCLOCKWISE);
            break;
        case 180:
            rotate(image, image, ROTATE_180);
            break;
        case 270:
            rotate(image, image, ROTATE_90_CLOCKWISE);
            break;
        default:
            break;
    }
}

//cvt color and adjust image, since cell phone camera preview form
Mat cvtFrame2MatWithYCrCb(const jbyte * image,int32_t frameWidth,int32_t frameHeight,int32_t rotationDegrees){
    //get images and cvt it
    Mat mat(frameHeight + frameHeight / 2, frameWidth, CV_8UC1, (uchar *) image);
    //cvt color to normal color and be normal size
    cvtColor(mat, mat, COLOR_YUV2RGB_NV21);
    //then wo can do it
    resize(mat,mat,Size(mat.cols/2,mat.rows/2),0,0,INTER_LINEAR);
    //rotate for rotatable phone
    rotateImage(mat, rotationDegrees);
    //cvt for skin color
    cvtColor(mat, mat, COLOR_RGB2YCrCb);
    //image for face tracking
    return mat;
}

//calc hist of color in roi for tracking face
void calcHists(){
    //let useless pixels be useless
    Mat mask;
    //tell you what is usefull
    inRange(oldImage, Scalar(0,133,77), Scalar(255,173,127),mask);
    //just for settling down the image
    Mat dst = Mat::zeros(oldImage.size(), oldImage.type());
    //which channels will be settled down
    int from_to[] = {1,1,2,2};
    //do it
    mixChannels(&oldImage,1, &dst,1,from_to,2);
    //calc hist of each face roi
    for (Rect rect: lastRects) {
        //will be result
        Mat hist;
        //get roi for not calc other part of image
        Mat dst_roi=Mat(dst,rect);
        //calc it
        calcHist(&dst_roi,1,channels,Mat(mask,rect),hist,2,histSize,ranges);
        //for gray
        normalize(hist, hist, 0, 255, NORM_MINMAX);
        //restore it in my data structure
        histKeeper.Restore(hist);
    }
    //be good for memory, maybe useless
    mask.release();
}

//rectangle to rect, 0-1 to 0-xxx, java to cv
Rect Rtg2Rct(JNIEnv * env, jobject face){
    jfloat x = env->CallFloatMethod(face,rtg_x);
    jfloat y = env->CallFloatMethod(face,rtg_y);
    jfloat w = env->CallFloatMethod(face,rtg_w);
    jfloat h = env->CallFloatMethod(face,rtg_h);
    return Rect(Point(x*fullWidth,y*fullHeight),
            Size(w*fullWidth,h*fullHeight));
}

//rect to rectangle, cv to java
jobject Rct2Rtg(JNIEnv * env, Rect& rect){
    return env->NewObject(cls_rtg,rtg_init,
    rect.x/(float)fullWidth,rect.y/(float)fullHeight,
    rect.width/(float)fullWidth,rect.height/(float)fullHeight);
}

//let fucking rect be in image, for not bugging
void RestrainRect(Rect& rect){
    rect.x = rect.x<0? 0:rect.x;
    rect.y = rect.y<0? 0:rect.y;
    rect.width = (rect.x+rect.width>fullWidth)? (fullWidth-rect.x):rect.width;
    rect.height = (rect.y+rect.height>fullHeight)? (fullHeight-rect.y):rect.height;
}

//let rect larger
void EnlargeRect(Rect& rect, int i) {
	rect.x -= i;
	rect.y -= i;
	rect.width += 2 * i;
	rect.height += 2 * i;
	//not too large to bugging
	RestrainRect(rect);
}

//how many rect will be involved
int stable_index = 3;
//make rect stable in time scale
void RectStabilizer(Rect& rect, std::vector<Rect>& stable_set) {
    //number of rect collection
    int stable_w = stable_set.size();
    //if empty, we add it
    if(!stable_w){
        stable_set.push_back(rect);
    }
    //if has rect, we calc weight mean
    else {
        //save this one
        stable_set.push_back(rect);
        //update size
        stable_w++;
        //init argument
        int x=0, y=0;
        //such as x = n*x_n + ... + i*x_i + ... 1*x_1;
        for(int i=0; i<stable_w; i++){
            x += (i+1)*stable_set[i].x;
            y += (i+1)*stable_set[i].y;
        }
        //and be mean as x = x / (0.5n(n+1));
        int index_sum = stable_w*(stable_w+1)/2;
        rect.x = x/index_sum;
        rect.y = y/index_sum;
        //remove if too much rect collected
        if(stable_w > stable_index)
            stable_set.erase(stable_set.begin());
    }
}

//jstring to char*, java to cpp, copy from Internet
char* jstringToChar(JNIEnv* env, jstring jstr) {
    char* rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("GB2312");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr = (jbyteArray) env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte* ba = env->GetByteArrayElements(barr, JNI_FALSE);
    if (alen > 0) {
        rtn = (char*) malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);
    return rtn;
}

//start the face track task, prepare for tracking
JNIEXPORT void JNICALL Java_com_calculator_liuxuncheng_emotiontracker_FaceTrackProccesor_startTrack
(JNIEnv * env, jobject obj, jbyteArray image, jint width, jint height, jint rotation, jobject rectangles, jstring eye_path){
    //load eye detect classifier
    eyeClassifier.load(jstringToChar(env, eye_path));
    //cvt [B 2 b* of image
    jbyte * image_array=env->GetByteArrayElements(image,0);
    //cvt color for skin color, and cvt ot normal size
    oldImage = cvtFrame2MatWithYCrCb(image_array,width,height,rotation);
    //release image
    env->ReleaseByteArrayElements(image, image_array, 2);
    //collect base data
    fullWidth = oldImage.cols;
    fullHeight = oldImage.rows;

    //******************
    //get jni all info
    //******************
    //get java class ArrayList<Rectangle>
    jclass cls_arraylist_tmp = env->GetObjectClass(rectangles);
    //make class global
    cls_arraylist = (jclass)env->NewGlobalRef(cls_arraylist_tmp);
    //get java Rectangle ArrayList<Rectangle>.get(int i)
    arraylist_get = env->GetMethodID(cls_arraylist,"get","(I)Ljava/lang/Object;");
    //get java int ArrayList<Rectangle>.size()
    arraylist_size = env->GetMethodID(cls_arraylist,"size","()I");
    //int len = (ArrayList<Rectangle>)rectangles.size()
    jint len = env->CallIntMethod(rectangles,arraylist_size);
    //get one object as obj_user = (ArrayList<Rectangle>)rectangles.get(0)
    jobject obj_user = env->CallObjectMethod(rectangles,arraylist_get,0);
    //get java class Rectangle, that is the class of list items
    jclass cls_rtg_tmp = env->GetObjectClass(obj_user);
    //make class global
    cls_rtg = (jclass)env->NewGlobalRef(cls_rtg_tmp);
    //get java float Rectangle.getX()|getY()|getWidth()|getHeight()
    rtg_x = env->GetMethodID(cls_rtg,"getX","()F");
    rtg_y = env->GetMethodID(cls_rtg,"getY","()F");
    rtg_w = env->GetMethodID(cls_rtg,"getWidth","()F");
    rtg_h = env->GetMethodID(cls_rtg,"getHeight","()F");
    //get java init method Rectangle(float x, float y, float w, float h)
    rtg_init = env->GetMethodID(cls_rtg,"<init>","(FFFF)V");
    //get java init method ArrayList<Rectangle>()
    aylst_init = env->GetMethodID(cls_arraylist,"<init>","()V");
    //get java boolean ArrayList<Rectangle>.add(Rectangle item)
    aylst_add = env->GetMethodID(cls_arraylist,"add","(Ljava/lang/Object;)Z");
    //get java class FaceTrackArgs as return type
    jclass r_class_tmp = env->FindClass("com/calculator/liuxuncheng/emotiontracker/FaceTrackArgs");
    //make class global
    r_class = (jclass)env->NewGlobalRef(r_class_tmp);
    //get java field (ArrayList<Rectangle>)FaceTrackArgs.Faces
    r_Faces = env->GetFieldID(r_class,"Faces","Ljava/util/List;");
    //get java field (int[])FaceTrackArgs.ROIstatuses
    r_ROIstatuses = env->GetFieldID(r_class,"ROIstatuses","[I");
    //get java field (int[])FaceTrackArgs.Optistatuses
    r_Optistatuses = env->GetFieldID(r_class,"Optistatuses","[I");
    //get java field (byte[][])FaceTrackArgs.ROIdatas
    r_ROIdatas = env->GetFieldID(r_class,"ROIdatas","[[B");
    //get java field (float[][])FaceTrackArgs.Optiflows
    r_Optiflows = env->GetFieldID(r_class,"Optiflows","[[F");
    //get java init method FaceTrackArgs()
    init_r = env->GetMethodID(r_class,"<init>","()V");

    //collect rect
    for(int i = 0; i<len; i++){
        //Rectangle obj_user = rectangles.get(i)
        jobject obj_user = env->CallObjectMethod(rectangles,arraylist_get,i);
        //Rect rect = (Rect)obj_user
        Rect rect = Rtg2Rct(env, obj_user);
        //kill zero
        if(rect.width==0||rect.height==0)continue;
        //save available rect
        lastRects.push_back(rect);
        //init eye_status
        eyeStatusKeeper.Restore(0);
        //init stable rect keeper
        stableSetKeeper.Restore(std::vector<Rect>());
    }
    //calc skin color hist of all rect
    calcHists();
    //ok for tracking
    hasStarted=1;
};

//base points of optical flow calc
std::vector<Point2f> optiflowBase;
//base ROI image of optical flow calc
Mat optiflowROI;
//size of optical flow ROI
Size optiRoiSize(110,130);
//start optical flow calc prepare, mark base points and save base ROI image
void startOptiflow(Mat roi, Point2f BaseX, Point2f BaseY, Point2f leftEye, Point2f rightEye) {
    //detect whether error invoke
	if (eyeStatusKeeper.Look())return;
	//get the rate of resizing
	float rate = (float)optiRoiSize.width / (float)roi.cols;
	//resize for uniform size
	resize(roi, roi, optiRoiSize);
	//cvt base vector of points
	BaseX *= rate;
	BaseY *= rate;
	leftEye *= rate;
	rightEye *= rate;
	//cvt color to gray, but as small image, so it is fast
	cvtColor(roi, roi, COLOR_RGB2GRAY);
	//save it as base image
	optiflowROI = roi;
	//all base point, l = left, r = right, e = eye, m = middle|mouth
	//eb = eyebrow, el = eyelid, f = face, t = top, b = below
	Point2f le_meb = 0.3 * BaseY + leftEye;
	Point2f re_meb = 0.3 * BaseY + rightEye;
	Point2f le_tel = 0.15 * BaseY + leftEye;
	Point2f re_tel = 0.15 * BaseY + rightEye;
	Point2f le_ieb = 0.2 * BaseY + 0.2 * BaseX + leftEye;
	Point2f re_ieb = 0.2 * BaseY - 0.2 * BaseX + rightEye;
	Point2f le_oeb = 0.2 * BaseY - 0.2 * BaseX + leftEye;
	Point2f re_oeb = 0.2 * BaseY + 0.2 * BaseX + rightEye;
	Point2f le_oe = -0.25 * BaseX + leftEye;
	Point2f re_oe = +0.25 * BaseX + rightEye;
	Point2f me_meb = 0.2 * BaseY + 0.5 * BaseX + leftEye;
	Point2f le_f = -0.5 * BaseY + leftEye;
	Point2f re_f = -0.5 * BaseY + rightEye;
	Point2f m_tm = -1 * BaseY + 0.5 * BaseX + leftEye;
	Point2f m_tlm = -1.05 * BaseY + 0.23 * BaseX + leftEye;
	Point2f m_trm = -1.05 * BaseY - 0.23 * BaseX + rightEye;
	Point2f m_bm = -1.4 * BaseY + 0.5 * BaseX + leftEye;
	Point2f m_blm = -1.35 * BaseY + 0.23 * BaseX + leftEye;
	Point2f m_brm = -1.35 * BaseY - 0.23 * BaseX + rightEye;
	Point2f m_lm = -1.2 * BaseY + leftEye;
	Point2f m_rm = -1.2 * BaseY + rightEye;
	//save all base point
	optiflowBase.push_back(le_meb);
	optiflowBase.push_back(re_meb);
	optiflowBase.push_back(le_tel);
	optiflowBase.push_back(re_tel);
	optiflowBase.push_back(le_ieb);
	optiflowBase.push_back(re_ieb);
	optiflowBase.push_back(le_oeb);
	optiflowBase.push_back(re_oeb);
	optiflowBase.push_back(le_oe);
	optiflowBase.push_back(re_oe);
	optiflowBase.push_back(me_meb);
	optiflowBase.push_back(le_f);
	optiflowBase.push_back(re_f);
	optiflowBase.push_back(m_tm);
	optiflowBase.push_back(m_tlm);
	optiflowBase.push_back(m_trm);
	optiflowBase.push_back(m_bm);
	optiflowBase.push_back(m_blm);
	optiflowBase.push_back(m_brm);
	optiflowBase.push_back(m_lm);
	optiflowBase.push_back(m_rm);
}

//calc optical flow between images of new ROI and base ROI in base points
std::vector<float> calcOptiflow(Mat roi) {
    //prevent error invoke, no eye position, no optical flow
	if (!eyeStatusKeeper.Look())return std::vector<float>();
	//prepare ROI
	resize(roi, roi, optiRoiSize);
    cvtColor(roi, roi, COLOR_RGB2GRAY);
    //vector of float store all flow components
	std::vector<float> r;
	//just new base point will be
	std::vector<Point2f> pts;
	//for calc status and err
	std::vector<uchar> status;
	std::vector<float> err;
	//calc it and be fast
	calcOpticalFlowPyrLK(optiflowROI, roi, optiflowBase, pts, status, err,Size(7,7),3,TermCriteria(TermCriteria::COUNT,32,1));
	//calc flow components
	for (int i = 0; i < pts.size(); i++) {
		r.push_back(pts[i].x - optiflowBase[i].x);
		r.push_back(pts[i].y - optiflowBase[i].y);
	}
	//update base
	optiflowBase = pts;
	optiflowROI = roi;
	//return flow components
	return r;
}

//track face -> find eyes -> prepare calc optical flow -> calc optical flow -> sent back all result
JNIEXPORT jobject JNICALL Java_com_calculator_liuxuncheng_emotiontracker_FaceTrackProccesor_trackFaces
(JNIEnv * env, jobject obj, jbyteArray image, jint width, jint height, jint rotation, jintArray optiflowControl, jintArray roiControl){
    //get control array for whether calc optical flow and sent ROI
    jint * opti_cons;
    jint * roi_cons;
    opti_cons = env->GetIntArrayElements(optiflowControl,NULL);
    roi_cons = env->GetIntArrayElements(roiControl,NULL);
    //the status of whether return flow and ROI
    int size_r = lastRects.size();
    jint opti_rs[20] = {0};
    jint roi_rs[20] = {0};
    //data of flow and roi for return
    jfloat * opti_data[size_r];
    jint opti_data_len[20] = {0};
    jbyte * roi_data[size_r];
    jint roi_data_len[20] = {0};
    //prevent error invoke
    if(!hasStarted)return nullptr;
    //get image
    jbyte * image_array=env->GetByteArrayElements(image,0);
    //image pretreatment
    Mat nv21_new = cvtFrame2MatWithYCrCb(image_array,width,height,rotation);
    //release image
    env->ReleaseByteArrayElements(image, image_array, 2);
    //prepare for return: faces = new ArrayList<Rectangle>
    jobject faces=env->NewObject(cls_arraylist,aylst_init);
    //rects for save new rect of face and update lastRects
    std::vector<Rect> rects;
    //term criteria of camshift for tracking face
    TermCriteria termCriteria(TermCriteria::EPS | TermCriteria::COUNT,32,1);
    //emmm, for in case
    fullWidth = nv21_new.cols;
    fullHeight = nv21_new.rows;
    //all iterator be first
    histKeeper.BeFirst();
    leyeKeeper.BeFirst();
    reyeKeeper.BeFirst();
    eyeStatusKeeper.BeFirst();
    stableSetKeeper.BeFirst();
    int i_cons = 0;
    //deal all face in rects
    for(auto it=lastRects.begin();it!=lastRects.end();it++){
        //get one rect of face
        Rect rect = *it;
        //let it be available and shut bug down
        RestrainRect(rect);
        //result of back project in future
        Mat dst;
        //hist of skin color of this face
        Mat hist = histKeeper.Look();
        //calc back project and be ok for code below
        calcBackProject(&nv21_new,1,channels,hist,dst,ranges,1);
        //move iterator
        histKeeper.Next();
        //camshift and rect will be update
        CamShift(dst,rect,termCriteria);
        //no face is neck.....
        rect.height=(int)(rect.width*1.35);
        //be stable
        RectStabilizer(rect, stableSetKeeper.Look());
        //be next one
        stableSetKeeper.Next();
        //save rect
        rects.push_back(rect);
        //save as Rectangle to faces: faces.add((Rectangle)rect)
        env->CallObjectMethod(faces,aylst_add,Rct2Rtg(env, rect));
        //eye detect and calc optical flow
        if (eyeStatusKeeper.Look() == 0) {
            //no eye detect, so detect it now, and ignored control code
            std::vector<Rect> eyes;
            //rect be larger
            EnlargeRect(rect, 5);
            //just one image can be detect
            Mat image_GRAY;
            //cvt to ok color
            cvtColor(nv21_new, image_GRAY, COLOR_YCrCb2BGR);
            //detect eyes
            eyeClassifier.detectMultiScale(Mat(image_GRAY, rect), eyes);
            //if two eyes be detect, then mark optical base point and save and sent ROI image
            if (eyes.size() == 2) {
                //eye is central rect of eye
                Point2f leftEye(eyes[0].x + eyes[0].width / 2.0, eyes[0].y + eyes[0].height / 2.0);
                Point2f rightEye(eyes[1].x + eyes[1].width / 2.0, eyes[1].y + eyes[1].height / 2.0);
                //get right order
                if (leftEye.x > rightEye.x) {
                    Point2f t = leftEye;
                    leftEye = rightEye;
                    rightEye = t;
                }
                //save eye positions and not detect again, as detect will take around 20 ms
                leyeKeeper.Insert(leftEye);
                reyeKeeper.Insert(rightEye);
                //get our face base vectors
                Point2f BaseX = rightEye - leftEye;
                Point2f BaseY(BaseX.y, -BaseX.x);
                //ROI points of ROI rect diagonal
                Point2f f_lt(leftEye.x - 0.6 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y), leftEye.y - 0.8 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y));
                Point2f f_rb(leftEye.x + 1.6 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y), leftEye.y + 1.8 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y));
                //ROI rect offset in all image
                Point offset(rect.x, rect.y);
                //ROI rect
                Rect roi(offset + (Point)f_lt, offset + (Point)f_rb);
                //avoid fuck roi
                RestrainRect(roi);
                //ROI image in color YCrCb
                Mat face_roi(nv21_new, roi);
                //be a normal color
	            cvtColor(face_roi, face_roi, COLOR_YCrCb2RGB);
	            //save ROI as png byte array
                std::vector<uchar> data_encode;
                std::vector<int> param = std::vector<int>(2);
                param[0] = IMWRITE_PNG_COMPRESSION;
                param[1] = 3;
                imencode(".png", face_roi, data_encode, param);
                //save encode data for return to java
                roi_data[i_cons] = (jbyte *)data_encode.data();
                roi_data_len[i_cons] = data_encode.size();
                roi_rs[i_cons] = 1;
	            //prepare calc optical flow
                startOptiflow(face_roi, BaseX, BaseY, leftEye - f_lt, rightEye - f_lt);
                eyeStatusKeeper.Updata(1);
            }
        }
        //if we had eyes position, we are controlled by input control code
        else {
            //if control code tell us something to do
            if(roi_cons[i_cons]==1 || opti_cons[i_cons]==1){
                //get ROI base vector
                auto leftEye = leyeKeeper.Look();
                Point2f BaseX = reyeKeeper.Look() - leyeKeeper.Look();
                Point2f BaseY(BaseX.y, -BaseX.x);
                //ROI point of ROI rect diagonal
                Point2f f_lt(leftEye.x - 0.6 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y), leftEye.y - 0.8 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y));
                Point2f f_rb(leftEye.x + 1.6 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y), leftEye.y + 1.8 * sqrt(BaseX.x * BaseX.x + BaseX.y * BaseX.y));
                //ROI rect offset in all image
                Point offset(rect.x, rect.y);
                //ROI rect
                Rect roi(offset + (Point)f_lt, offset + (Point)f_rb);
                //avoid fuck roi
                RestrainRect(roi);
                //ROI image
                Mat face_roi(nv21_new, roi);
                //be a normal color again
                cvtColor(face_roi, face_roi, COLOR_YCrCb2RGB);
                //if control code tell us to sent back ROI image, then be png
                if(roi_cons[i_cons]==1){
                    //save ROI as png
                    std::vector<uchar> data_encode;
                    std::vector<int> param = std::vector<int>(2);
                    param[0] = IMWRITE_PNG_COMPRESSION;
                    param[1] = 3;
                    imencode(".png", face_roi, data_encode, param);
                    //save encode data for return to java
                    roi_data[i_cons] = (jbyte *)data_encode.data();
                    roi_data_len[i_cons] = data_encode.size();
                    roi_rs[i_cons] = 1;
                }
                //if control code tell us to sent back flow, then calc and sent it
                if(opti_cons[i_cons]==1){
                    //calc optical flow
                    std::vector<float> flows = calcOptiflow(face_roi);
                    //save flow for return to java
                    opti_data[i_cons] = flows.data();
                    opti_data_len[i_cons] = flows.size();
                    opti_rs[i_cons] = 1;
                }
            }
        }
        //update iterators
        eyeStatusKeeper.Next();
        leyeKeeper.Next();
        reyeKeeper.Next();
        i_cons++;
    }
    //update base images for face tracking
    oldImage=nv21_new;
    lastRects=rects;

    //*******************
    //release array
    //*******************
    env->ReleaseIntArrayElements(optiflowControl, opti_cons, 2);
    env->ReleaseIntArrayElements(roiControl, roi_cons, 2);

    //*******************
    //build return value
    //*******************

    //FaceTrackArgs r = new FaceTrackArgs()
    jobject r = env->NewObject(r_class,init_r);

    //r.Faces = faces
    env->SetObjectField(r,r_Faces,faces);

    //r.ROIstatuses
    //roi_jni_rs = new int[]
    jintArray roi_jni_rs = env->NewIntArray(size_r);
    //roi_jni_rs << roi_rs
    env->SetIntArrayRegion(roi_jni_rs, 0, size_r, roi_rs);
    //r.ROIstatuses = roi_jni_rs
    env->SetObjectField(r,r_ROIstatuses,roi_jni_rs);

    //r.ROIdatas
    //get class byte[]
    jclass clsByteArray = env->FindClass("[B");
    //roi_jni_data = new (byte[])[]
    jobjectArray roi_jni_data = env->NewObjectArray(size_r,clsByteArray,NULL);
    //assignment of each one
    for (int i = 0; i < size_r; i++)
    {
        //byteArr = new byte[]
        jbyteArray byteArr = env->NewByteArray(roi_data_len[i]);
        //byteArr << roi_data[i]
        env->SetByteArrayRegion(byteArr, 0, roi_data_len[i], roi_data[i]);
        //roi_jni_data[i] = byteArr
        env->SetObjectArrayElement(roi_jni_data, i, byteArr);
        //delete byteArr
        env->DeleteLocalRef(byteArr);
    }
    //r.ROIdatas = roi_jni_data
    env->SetObjectField(r,r_ROIdatas,roi_jni_data);

    //r.Optistatuses
    //opti_jni_rs = new int[]
    jintArray opti_jni_rs = env->NewIntArray(size_r);
    //opti_jni_rs << opti_rs
    env->SetIntArrayRegion(opti_jni_rs, 0, size_r, opti_rs);
    //r.Optistatuses = opti_jni_rs
    env->SetObjectField(r,r_Optistatuses,opti_jni_rs);

    //r.Optiflows
    //get java class float[]
    jclass clsFloatArray = env->FindClass("[F");
    //opti_jni_data = new (float[])[]
    jobjectArray opti_jni_data = env->NewObjectArray(size_r,clsFloatArray,NULL);
    //assignment of each one
    for (int i = 0; i < size_r; i++)
    {
        //floatArr = new float[]
        jfloatArray floatArr = env->NewFloatArray(opti_data_len[i]);
        //floatArr << opti_data[i]
        env->SetFloatArrayRegion(floatArr, 0, opti_data_len[i], opti_data[i]);
        //opti_jni_data[i] = floatArr
        env->SetObjectArrayElement(opti_jni_data, i, floatArr);
        //delete floatArr
        env->DeleteLocalRef(floatArr);
    }
    //r.Optiflows = opti_jni_data
    env->SetObjectField(r,r_Optiflows,opti_jni_data);

    //return FaceTrackArgs
    return r;
};

//reset all my iterator
JNIEXPORT void JNICALL Java_com_calculator_liuxuncheng_emotiontracker_FaceTrackProccesor_reset
(JNIEnv *, jobject){
    histKeeper.DropAll();
    hasStarted = 0;
    lastRects.clear();
    optiflowBase.clear();
    eyeStatusKeeper.DropAll();
    reyeKeeper.DropAll();
    leyeKeeper.DropAll();
    stableSetKeeper.DropAll();
}