#include "stdafx.h"
#include "HazeRemoval.h"

//构造函数
HazeRemoval::HazeRemoval(IplImage* in,int r1,double eps1)
{
	InputImage = in;
	Black = cvCreateImage(cvGetSize(in),IPL_DEPTH_8U,1);
	DarkChannel = cvCreateImage(cvGetSize(in),IPL_DEPTH_8U,1);
	patchSize = 15;
	patch = cvCreateStructuringElementEx(patchSize,patchSize,(patchSize-1)/2,(patchSize-1)/2,CV_SHAPE_RECT);
	transmission_map = cvCreateImage(cvGetSize(in),IPL_DEPTH_32F,1);
	w = 0.95;
	HazeFreeImage = cvCreateImage(cvGetSize(in),IPL_DEPTH_8U,3);
	t0 = 0.1;
	NormalizeBlack = cvCreateImage(cvGetSize(in),IPL_DEPTH_32F,1);
	I = cvCreateImage(cvGetSize(in),8,1);
	r = r1;
	eps = eps1;
}

//析构函数
HazeRemoval::~HazeRemoval()
{
	cvReleaseImage(&Black);
	cvReleaseImage(&DarkChannel);
	cvReleaseStructuringElement(&patch);
	cvReleaseImage(&transmission_map);
	cvReleaseImage(&HazeFreeImage);
	cvReleaseImage(&NormalizeBlack);
	cvReleaseImage(&I);
	//delete guidedfilter;
}

IplImage* HazeRemoval::GetDarkChannel()
{
	return DarkChannel;
}

IplImage* HazeRemoval::GetTransmissionMap()
{
	return transmission_map;
}

IplImage* HazeRemoval::GetTransmissionMapRefine()
{
	return transmission_mapRefine;
}

IplImage* HazeRemoval::GetHazeFreeImage()
{
	return HazeFreeImage;
}

void HazeRemoval::SetR(int r1)
{
	r = r1;
}

void HazeRemoval::SetEps(double eps1)
{
	eps = eps1;
}

//计算暗通道
void HazeRemoval::CalculateDarkChannel()
{
	for (int y=0;y<InputImage->height;y++)
	{
		for (int x=0;x<InputImage->width;x++)
		{
			int r,g,b;
			CvScalar pixel = cvGet2D(InputImage,y,x);
			b = pixel.val[0];
			g = pixel.val[1];
			r = pixel.val[2];
			int min = Min(r,g,b);
			cvSetReal2D(Black,y,x,min);
		}
	}
	//对图像Black做最小滤波（即腐蚀操作）
	cvErode(Black,DarkChannel,patch,1);
}

//求三个数的最小值
int HazeRemoval::Min(int r,int g,int b)
{
	r = (r<=g)?r:g;
	b = (b<=r)?b:r;
	return b;
}

//求三个浮点数的最小值
float HazeRemoval::MinFloat(float r,float g,float b)
{
	r = (r<=g)?r:g;
	b = (b<=r)?b:r;
	return b;
}

//计算标准化雾图I(y)/A的暗通道
void HazeRemoval::CalculateNormalizeHazeImgDarkChannel()
{
	for (int y=0;y<InputImage->height;y++)
	{
		for (int x=0;x<InputImage->width;x++)
		{
			float r,g,b;
			CvScalar pixel = cvGet2D(InputImage,y,x);
			b = pixel.val[0]/A.val[0];
			g = pixel.val[1]/A.val[1];
			r = pixel.val[2]/A.val[2];
			float min = MinFloat(r,g,b);
			cvSetReal2D(NormalizeBlack,y,x,min);
		}
	}
	//对NormalizeBlack做归一化处理
	cvNormalize(NormalizeBlack,NormalizeBlack,1.0,0.0,CV_C);
	//对图像NormalizeBlack做最小滤波（即腐蚀操作）
	cvErode(NormalizeBlack,NormalizeBlack,patch,1);
}


//估计透射率
void HazeRemoval::Estimated_transmission()
{
	CalculateNormalizeHazeImgDarkChannel();
	for (int y=0;y<NormalizeBlack->height;y++)
	{
		for (int x=0;x<NormalizeBlack->width;x++)
		{
			float t;
			t = 1-w*cvGetReal2D(NormalizeBlack,y,x);
			cvSetReal2D(transmission_map,y,x,t);
		}
	}

	//导向滤波
	cvCvtColor(InputImage,I,CV_BGR2GRAY);
	guidedfilter = new GuidedFilter(I,transmission_map,r,eps);
	transmission_mapRefine = guidedfilter->Getq();
}

//快速排序（从小到大）
void HazeRemoval::quick_sort(IplImage* s,IplImage* sx,IplImage* sy,int l,int r)//s是1*n
{
	if (l<r)
	{
		int i=l,j=r,x = cvGetReal2D(s,0,l),xx = cvGetReal2D(sx,0,l),yy = cvGetReal2D(sy,0,l);
		while (i<j)
		{
			while(i<j&&cvRound(cvGetReal2D(s,0,j))>=x) // 从右向左找第一个小于x的数
				j--;  
			if(i<j) 
			{
				cvSetReal2D(s,0,i,cvGetReal2D(s,0,j));
				cvSetReal2D(sx,0,i,cvGetReal2D(sx,0,j));
				cvSetReal2D(sy,0,i,cvGetReal2D(sy,0,j));
				i++;
			}

			while(i<j&&cvRound(cvGetReal2D(s,0,i))<x) // 从左向右找第一个大于等于x的数
				i++;  
			if(i<j) 
			{
				cvSetReal2D(s,0,j,cvGetReal2D(s,0,i));
				cvSetReal2D(sx,0,j,cvGetReal2D(sx,0,i));
				cvSetReal2D(sy,0,j,cvGetReal2D(sy,0,i));
				j--;
			}
		}
		cvSetReal2D(s,0,i,x);
		cvSetReal2D(sx,0,i,xx);
		cvSetReal2D(sy,0,i,yy);
		quick_sort(s,sx,sy,l,i-1); 
		quick_sort(s,sx,sy,i+1,r);
	}
}

//估计大气光照
void HazeRemoval::Eatimate_A()
{
	int num = cvRound(DarkChannel->height*DarkChannel->width*0.001);
	IplImage* s = cvCreateImage(cvSize(DarkChannel->height*DarkChannel->width,1),8,1);
	IplImage* sx = cvCreateImage(cvSize(DarkChannel->height*DarkChannel->width,1),8,1);
	IplImage* sy = cvCreateImage(cvSize(DarkChannel->height*DarkChannel->width,1),8,1);
	int e = 0;
	int max = 0;
	for (int y=0;y<DarkChannel->height;y++)
	{
		for (int x=0;x<DarkChannel->width;x++)
		{
			if (max<cvRound(cvGetReal2D(DarkChannel,y,x)))
			{
				max = cvRound(cvGetReal2D(DarkChannel,y,x));
			}
		}
	}
	for (int y=0;y<DarkChannel->height;y++)
	{
		for (int x=0;x<DarkChannel->width;x++)
		{
			if (max==cvRound(cvGetReal2D(DarkChannel,y,x)))
			{
				cvSetReal2D(s,0,e,cvGetReal2D(DarkChannel,y,x));
				cvSetReal2D(sx,0,e,x);
				cvSetReal2D(sy,0,e,y);
				e++;
			}
		}
	}
	long sumAb = 0;
	long sumAg = 0;
	long sumAr = 0;
	int cnt = 0;
	for (int i=0;i<num&&i<e;i++)
	{
		sumAb += cvRound(cvGet2D(InputImage,cvGetReal2D(sy,0,i),cvGetReal2D(sx,0,i)).val[0]);
		sumAg += cvRound(cvGet2D(InputImage,cvGetReal2D(sy,0,i),cvGetReal2D(sx,0,i)).val[1]);
		sumAr += cvRound(cvGet2D(InputImage,cvGetReal2D(sy,0,i),cvGetReal2D(sx,0,i)).val[2]);
		cnt++;
		//cvSet2D(InputImage,cvGetReal2D(sy,0,i),cvGetReal2D(sx,0,i),CV_RGB(255,0,0));
	}
	A.val[0] = sumAb/cnt;
	A.val[1] = sumAg/cnt;
	A.val[2] = sumAr/cnt;
	if (A.val[0]>230||A.val[0]<140)
	{
		A.val[0] = 220;
	}
	if (A.val[1]>230||A.val[1]<140)
	{
		A.val[1] = 220;
	}
	if (A.val[2]>230||A.val[2]<140)
	{
		A.val[2] = 220;
	}
}

//恢复无雾图像
void HazeRemoval::Recover()
{
	for (int y=0;y<HazeFreeImage->height;y++)
	{
		for (int x=0;x<HazeFreeImage->width;x++)
		{
			float r = 0,g = 0,b = 0;
			float t = cvGetReal2D(transmission_mapRefine,y,x);
			CvScalar pixel_in = cvGet2D(InputImage,y,x);
			b = pixel_in.val[0];
			g = pixel_in.val[1];
			r = pixel_in.val[2];
			CvScalar pixel_hazefree;
			pixel_hazefree.val[0] = (b-A.val[0])/MaxTwo(t,t0)+A.val[0];
			pixel_hazefree.val[1] = (g-A.val[1])/MaxTwo(t,t0)+A.val[1];
			pixel_hazefree.val[2] = (r-A.val[2])/MaxTwo(t,t0)+A.val[2];
			cvSet2D(HazeFreeImage,y,x,pixel_hazefree);
		}
	}
}

//求两个浮点数的最大值
float HazeRemoval::MaxTwo(float a,float b)
{
	return a>b?a:b;
}
