//
// Created by Mechrevo on 2019/8/14.
//

#ifndef EMOTIONTRACKER_DATAKEEPER_H
#define EMOTIONTRACKER_DATAKEEPER_H


template <class T> class DataKeeper {
public:
    std::vector<T> datas;
    int currentPtr = 0;
    DataKeeper() {
    };
    void Restore(T data) {datas.push_back(data);};
	void BeFirst() { currentPtr = 0; };
    void BeLast() {currentPtr = datas.size() - 1;};
    T& Look() {
		if (currentPtr >= datas.size())currentPtr -= datas.size();
		if (currentPtr >= datas.size())currentPtr = 0;
        return datas[currentPtr];
    };
    void Drop(){
        datas.erase(datas.begin() + currentPtr);
    };
    void DropAll(){
        datas.clear();
        currentPtr = 0;
    };
    void Next(){
		currentPtr++;
    };
	void Insert(T data) { datas.insert(datas.begin() + currentPtr, data); }
	void Updata(T data) { datas[currentPtr] = data; }
};



#endif //EMOTIONTRACKER_DATAKEEPER_H
