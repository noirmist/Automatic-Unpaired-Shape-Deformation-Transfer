﻿#include "new_ARAPDeform.h"

#include <queue>
#include <OpenMesh/Core/IO/MeshIO.hh>

#include <iostream>
#include <fstream>
#include <algorithm>
#include "SolveOpt.h"


using namespace std;
using namespace Eigen;

#include <omp.h>

/* #ifdef DUSE_OPENMP
#define OMP_open __pragma(omp parallel num_threads(omp_get_num_procs()*2)) \
{ \
	__pragma(omp for)
#define OMP_end \
}
#else
#define OMP_open ;
#define OMP_end ;
#endif */

#define floatTime ((clock()-tt)*1.0 / CLOCKS_PER_SEC)


//ori
new_ARAPDeform::new_ARAPDeform(DTriMesh &refMesh, std::vector<DTriMesh*> ms, bool new_feature) : ref(refMesh), meshs(ms) {
	int cpunum = omp_get_num_procs();
	omp_set_num_threads(cpunum*2);

	//this->eng = &eng;

	fvs.resize(ms.size(), FeatureVector(*ms[0]));
	
#pragma omp parallel num_threads(omp_get_num_procs()*2)
{
	#pragma omp for
		for (int i = 0; i < fvs.size(); i++) {
			std::cout << "Calc " << i << " feature vector " << std::endl;

			ref.getFeature(*meshs[i], fvs[i]);
			//this->bfscorrot(fvs[i]);
			if (new_feature) {
				this->bfscorrot(fvs[i]);
			}

		}

}//OMP_end
}


void new_ARAPDeform::GetFeatureMat2(std::string & currentpath, std::string & inputfolder, std::string outputfolder, bool opt)
{

	int vertices_n_ = this->ref.mesh[0].n_vertices();
	int edges_n_ = this->ref.mesh[0].n_edges();
	//TODO:: trans R and L
	int * mxIrL = new int[edges_n_ * 2 * 2];
	int * mxJrL = new int[edges_n_ * 2 * 2];
	double * mxPrL = new double[edges_n_ * 2 * 2];
	memset(mxIrL, 0, edges_n_ * 2 * 2 * sizeof(int));
	memset(mxJrL, 0, edges_n_ * 2 * 2 * sizeof(int));
	memset(mxPrL, 0.0, edges_n_ * 2 * 2 * sizeof(double));
	//mxJrL[0] = 0;
	int nnzcount_I = 0;
	int nnzcount_j = 0;
	int nnzcount_J = 0;
	int nnzcount_P = 0;
	int K_dimension = 1;
	int edge_count = 0;
	std::vector<std::pair<int, int>> edge_adjacent(edges_n_ * 2, std::pair<int, int>(0, 0));
	//std::ofstream L_("L.txt");

	for (int i = 0; i < vertices_n_; i++)//base energy item
	{
		DTriMesh::VertexHandle vi(i);
		for (DTriMesh::VertexVertexIter vj = this->ref.mesh[0].vv_iter(vi); vj.is_valid(); vj++)
		{
			int j = vj->idx();
			for (int index = 0; index < K_dimension; index++)//x,y,z,3 axis
			{
				int jjj = j * K_dimension + index;
				int iii = i * K_dimension + index;
				//int hahaha = 2 * nnzcount_J;
				if (i > j)//point coefficient
				{
					mxIrL[nnzcount_I++] = jjj;
					mxIrL[nnzcount_I++] = iii;
					mxPrL[nnzcount_P++] = 1;
					mxPrL[nnzcount_P++] = -1;
					mxJrL[nnzcount_j++] = nnzcount_J;
					mxJrL[nnzcount_j++] = nnzcount_J;
					//edge_adjacentPr[edge_count++] = jjj;
					//edge_adjacentPr[edge_count++] = iii;
					edge_adjacent[edge_count].first = jjj;
					edge_adjacent[edge_count++].second = iii;
					//L_<<nnzcount_J-1<<" "<<jjj<<" "<<1<<std::endl;
					//L_<<nnzcount_J-1<<" "<<iii<<" "<<-1<<std::endl;

				}
				else {
					mxIrL[nnzcount_I++] = iii;
					mxIrL[nnzcount_I++] = jjj;
					mxPrL[nnzcount_P++] = -1;
					mxPrL[nnzcount_P++] = 1;
					mxJrL[nnzcount_j++] = nnzcount_J;
					mxJrL[nnzcount_j++] = nnzcount_J;
					//edge_adjacentPr[edge_count++] = iii;
					//edge_adjacentPr[edge_count++] = jjj;
					edge_adjacent[edge_count].first = iii;
					edge_adjacent[edge_count++].second = jjj;
					//L_<<nnzcount_J-1<<" "<<iii<<" "<<-1<<std::endl;
					//L_<<nnzcount_J-1<<" "<<jjj<<" "<<1<<std::endl;

				}
				//edge_adjacentPr[edge_count++]=iii;
				//edge_adjacentPr[edge_count++]=jjj;
				//mxJrL[nnzcount_J] = (mwIndex)hahaha;
				nnzcount_J++;
			}
		}
	}
	//L_.close();

	std::vector<Eigen::Vector3d> axis(vertices_n_);
	std::vector<double> angle(vertices_n_, 0);

	std::vector<Eigen::MatrixXd> R_Ptr(this->fvs.size());
	std::vector<Eigen::MatrixXd> S_Ptr(this->fvs.size());
	
	for (int i = 0; i < this->fvs.size(); i++)
	{
		//std::cout<<"start..."<<std::endl;
		//const std::string buffer = std::to_string(i+1);

		
		for (int j = 0; j < fvs[0].rots.size(); j++)
		{
			angle[j] = fvs[i].rots[j].theta + fvs[i].rots[j].circlek * 2 * M_PI;
			axis[j] = fvs[i].rots[j].axis;
		}

		std::vector<Rot> axis_anglenew(vertices_n_);
		if (i != 0)
		{
			if (opt==true)
			{
				axis_anglenew = SolveOpt(axis, angle, edge_adjacent, mxJrL, mxIrL, mxPrL);// time  is 120 and use init is false
				fvs[i].rots.assign(axis_anglenew.begin(),axis_anglenew.end());
			}
/* 			for (int j = 0;j < fvs[0].rots.size(); j++)
			{
			fvs[i].rots[j].axis = axis_anglenew[j].axis;

			fvs[i].rots[j].theta = axis_anglenew[j].theta;
			fvs[i].rots[j].circlek = 0;
			} */
			
		}

		std::cout << "finish " << i << " obj opt!!" << std::endl;

		std::vector<double> R_Ptr_(fvs[0].rots.size() * 9,0);
		std::vector<double> S_Ptr_(fvs[0].rots.size() * 9,0);
		//std::cout<<fvs[0].rots.size()<<std::endl;
		int tickR = 0;
		int tickS = 0;
		for (int j = 0; j < fvs[0].rots.size(); j++)
		{

			//fvs[i].rots[j].ToLogR();

			//fvs[i].rots[j].r = exp(fvs[i].rots[j].logr);

 			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(0, 0);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(0, 1);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(0, 2);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(1, 0);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(1, 1);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(1, 2);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(2, 0);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(2, 1);
			R_Ptr_[tickR++] = fvs[i].rots[j].logr.coeff(2, 2); 

			S_Ptr_[tickS++] = fvs[i].s[j].coeff(0, 0);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(0, 1);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(0, 2);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(1, 0);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(1, 1);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(1, 2);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(2, 0);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(2, 1);
			S_Ptr_[tickS++] = fvs[i].s[j].coeff(2, 2);
			//std::cout<<R_matrixPr[1]<<std::endl;
//std::cout<<j<<std::endl;
		}
		Eigen::MatrixXd RPtr = Eigen::VectorXd::Map(&R_Ptr_[0],R_Ptr_.size());
		Eigen::MatrixXd SPtr = Eigen::VectorXd::Map(&S_Ptr_[0],S_Ptr_.size());
		R_Ptr[i] = RPtr.transpose();
		S_Ptr[i] = SPtr.transpose();
	}
	//return;
	std::ofstream LOGRNEW;
	std::ofstream S;
	LOGRNEW.open(outputfolder+"/LOGRNEW.txt", std::ofstream::out);
	S.open(outputfolder+"/S.txt", std::ofstream::out);
	std::copy(R_Ptr.begin(),R_Ptr.end(),std::ostream_iterator<Eigen::MatrixXd>(LOGRNEW,"\n"));
	std::copy(S_Ptr.begin(),S_Ptr.end(),std::ostream_iterator<Eigen::MatrixXd>(S,"\n"));
	LOGRNEW.close();
	S.close();

	std::cout << "feature path(LOGRNEW):" << outputfolder+"/LOGRNEW.txt" << std::endl;
	std::cout << "feature path(S)      :" << outputfolder+"/S.txt" << std::endl;

}


new_ARAPDeform::~new_ARAPDeform()
{
	// destory
}


void new_ARAPDeform::bfscorrot(FeatureVector& fv)
{
	fv.rots.resize(fv.r.size());
	vector<bool> tovist(fv.r.size(), false);
	for (int i = 0; i < ref.bfsq.size(); i++) {
		int j = ref.bfsq[i].first;
		if (j == 1)
		{
			int a = 1;
		}
		int fa = ref.bfsq[i].second.first;
		int ei = ref.bfsq[i].second.second;
		//cout<<j<<" "<<fa<<endl;
		if (j == 2410 && fa == 2409)
		{
			/*cout<<j<<endl;*/
			int a = 1;
		}
		if (fa != -1)
		{
			assert(tovist[fa]);
			fv.rots[j] = logrot(fv.r[j], fv.rots[fa]);
			tovist[j] = true;


		}
		else
		{
			fv.rots[j] = logrot(fv.r[j]);
			tovist[j] = true;
		}
		//		cout<<j<<": "<<fv.rots[j].ToAngle()<<endl;
		Eigen::Matrix3d logr = log(fv.r[j]);
		//Eigen::Matrix3d r = exp(fv.rots[j].logr);
		//fv.rots[j].r = exp(fv.rots[j].logr);
		//double _normal = (fv.rots[j].logr-logr).squaredNorm();
		double _normal = (exp(fv.rots[j].logr) - fv.r[j]).squaredNorm();
		if (_normal > 0.0001)
		{
			//cout<<"error"<<endl;
		}
		Eigen::Matrix3d zero = Eigen::Matrix3d::Zero();
		if ((fv.rots[j].logr - zero).squaredNorm() > 1e-6)
		{
			fv.isrot[j] = true;
			fv.rot_vertices.insert(j);
		}
		fv.logr_plus_s[j] = fv.rots[j].logr + fv.s[j];
	}
}


