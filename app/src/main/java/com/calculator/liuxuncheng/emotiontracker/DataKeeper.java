package com.calculator.liuxuncheng.emotiontracker;

import java.util.ArrayList;

class DataKeeper<T> {
    ArrayList<T> datas;
    int currentPtr;
    DataKeeper(){
        datas = new ArrayList<T>();
        currentPtr = 0;
    }
    void Restore(T data){
        datas.add(data);
    }
    void BeFirst(){
        currentPtr = 0;
    }
    void BeLast(){
        currentPtr = datas.size() - 1;
    }
    T Look(){
        if(currentPtr>=datas.size())return null;
        return datas.get(currentPtr);
    }
    void Drop(){
        datas.remove(currentPtr--);
    }
    void DropAll(){
        datas = new ArrayList<T>();
        currentPtr = 0;
    }
    void Next(){
        currentPtr = currentPtr>=datas.size()? 0:currentPtr+1;
    }
}
