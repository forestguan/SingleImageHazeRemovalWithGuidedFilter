#include "Global.h"
#include "GuidedFilter.h"

class HazeRemoval
{
	//成员变量
private:
	IplImage* InputImage;//输入图像
	IplImage* Black;
	IplImage* DarkChannel;//暗通道图
	int patchSize;//patch的大小
	IplConvKernel* patch;//patch窗口
	IplImage* transmission_map;//透射率图[0,1]
	float w;
	CvScalar A;//大气光照
	IplImage* HazeFreeImage;//无雾图像
	float t0;
	IplImage* NormalizeBlack;
	IplImage* transmission_mapRefine;//使用导向滤波优化后的透射率图[0,1]
	IplImage* I;//导向图
	GuidedFilter* guidedfilter;//导向滤波
	int r;//导向滤波半径
	double eps;//导向滤波正则化系数(regularization parameter)

	//成员函数
public:
	HazeRemoval(IplImage* in,int r1,double eps1);//构造函数
	~HazeRemoval();//析构函数

	IplImage* GetDarkChannel();
	IplImage* GetTransmissionMap();
	IplImage* GetTransmissionMapRefine();
	IplImage* GetHazeFreeImage();

	void SetR(int r1);
	void SetEps(double eps1);

	void CalculateDarkChannel();//计算暗通道
	int Min(int r,int g,int b);//求三个数的最小值
	void Estimated_transmission();//估计透射率
	void quick_sort(IplImage* s,IplImage* sx,IplImage* sy,int l,int r);//快速排序（从小到大）
	void Eatimate_A();//估计大气光照
	void Recover();//恢复无雾图像
	float MaxTwo(float a,float b);//求两个浮点数的最大值
	void CalculateNormalizeHazeImgDarkChannel();//计算标准化雾图I(y)/A的暗通道
	float MinFloat(float r,float g,float b);//求三个浮点数的最小值
};
