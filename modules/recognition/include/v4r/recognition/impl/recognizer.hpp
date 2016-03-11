/******************************************************************************
 * Copyright (c) 2013 Aitor Aldoma
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ******************************************************************************/

/**
*
*      @author Aitor Aldoma
*      @author Thomas Faeulhammer (faeulhammer@acin.tuwien.ac.at)
*      @date Feb, 2013
*      @brief object instance recognizer
*/

#include <v4r/common/miscellaneous.h>
#include <v4r/common/convertCloud.h>
#include <v4r/common/convertNormals.h>
#include <v4r/recognition/recognizer.h>
#include <v4r/recognition/ghv.h>
#include <v4r/recognition/hypotheses_verification.h>
#include <v4r/recognition/voxel_based_correspondence_estimation.h>
#include <v4r/segmentation/multiplane_segmentation.h>
#include <v4r/segmentation/ClusterNormalsToPlanesPCL.h>

#include <pcl/common/centroid.h>
#include <pcl/filters/crop_box.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/icp.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <sstream>

namespace v4r
{

template<typename PointT>
void
Recognizer<PointT>::hypothesisVerification ()
{
    std::vector<typename pcl::PointCloud<PointT>::ConstPtr> aligned_models (models_.size ());
    std::vector<pcl::PointCloud<pcl::Normal>::ConstPtr> aligned_model_normals (models_.size ());

    hypothesis_is_verified_.clear();

    if(models_.empty())
    {
        std::cout << "No generated models to verify!" << std::endl;
        return;
    }

#pragma omp parallel for schedule(dynamic)
    for(size_t i=0; i<models_.size(); i++)
    {
        typename pcl::PointCloud<PointT>::Ptr aligned_model_tmp (new pcl::PointCloud<PointT>);
        pcl::PointCloud<pcl::Normal>::Ptr aligned_normal_tmp (new pcl::PointCloud<pcl::Normal>);
        ConstPointTPtr model_cloud = models_[i]->getAssembled ( param_.resolution_mm_model_assembly_ );
        pcl::transformPointCloud (*model_cloud, *aligned_model_tmp, transforms_[i]);
        aligned_models[i] = aligned_model_tmp;
        pcl::PointCloud<pcl::Normal>::ConstPtr normal_cloud_const = models_[i]->getNormalsAssembled (hv_algorithm_->getResolution());
        transformNormals(*normal_cloud_const, *aligned_normal_tmp, transforms_[i]);
        aligned_model_normals[i] = aligned_normal_tmp;
    }


    boost::shared_ptr<GHV<PointT, PointT> > hv_algorithm_ghv;
    if( hv_algorithm_ )
        hv_algorithm_ghv = boost::dynamic_pointer_cast<GHV<PointT, PointT>> (hv_algorithm_);

    hv_algorithm_->setOcclusionCloud (scene_);
    hv_algorithm_->setSceneCloud (scene_);
    hv_algorithm_->addModels (aligned_models, aligned_model_normals);

    if( hv_algorithm_ghv )
        hv_algorithm_ghv->setRequiresNormals(false);

    hv_algorithm_->verify ();
    hv_algorithm_->getMask (hypothesis_is_verified_);

    std::vector<boost::shared_ptr<Eigen::Matrix4f> > refined_transforms = hv_algorithm_->getRefinedTransforms();

    for(size_t i=0; i<refined_transforms.size(); i++)
    {
        if ( i < refined_transforms.size() && refined_transforms[i])
            transforms_[i] = *refined_transforms[i] * transforms_[i];
    }
}

template<typename PointT>
void
Recognizer<PointT>::visualize() const
{
    if(!vis_) {
        vis_.reset(new pcl::visualization::PCLVisualizer("single-view recognition results"));
        vis_->createViewPort(0,0,1,0.33,vp1_);
        vis_->createViewPort(0,0.33,1,0.66,vp2_);
        vis_->createViewPort(0,0.66,1,1,vp3_);
        vis_->addText("input cloud", 10, 10, 20, 1, 1, 1, "input", vp1_);
        vis_->addText("generated hypotheses", 10, 10, 20, 0, 0, 0, "generated hypotheses", vp2_);
        vis_->addText("verified hypotheses", 10, 10, 20, 0, 0, 0, "verified hypotheses", vp3_);
    }

    vis_->removeAllPointClouds();
    vis_->removeAllPointClouds(vp1_);
    vis_->removeAllPointClouds(vp2_);
    vis_->removeAllPointClouds(vp3_);

    typename pcl::PointCloud<PointT>::Ptr vis_cloud (new pcl::PointCloud<PointT>);
    pcl::copyPointCloud(*scene_, *vis_cloud);
    vis_cloud->sensor_origin_ = Eigen::Vector4f::Zero(4);
    vis_cloud->sensor_orientation_ = Eigen::Quaternionf::Identity();
    vis_->addPointCloud(vis_cloud, "input cloud", vp1_);
    vis_->setBackgroundColor(.0f, .0f, .0f, vp2_);

    for(size_t i=0; i<models_.size(); i++)
    {
        ModelT &m = *models_[i];
        const std::string model_id = m.id_.substr(0, m.id_.length() - 4);
        std::stringstream model_label;
        model_label << model_id << "_" << i;
        typename pcl::PointCloud<PointT>::Ptr model_aligned ( new pcl::PointCloud<PointT>() );
        typename pcl::PointCloud<PointT>::ConstPtr model_cloud = m.getAssembled( param_.resolution_mm_model_assembly_ );
        pcl::transformPointCloud( *model_cloud, *model_aligned, transforms_[i]);
        vis_->addPointCloud(model_aligned, model_label.str(), vp2_);
    }
    vis_->setBackgroundColor(.5f, .5f, .5f, vp2_);


    for(size_t i=0; i<models_.size(); i++)
    {
        if(!hypothesis_is_verified_[i])
            continue;

        ModelT &m = *models_[i];
        const std::string model_id = m.id_.substr(0, m.id_.length() - 4);
        std::stringstream model_label;
        model_label << model_id << "_v_" << i;
        typename pcl::PointCloud<PointT>::Ptr model_aligned ( new pcl::PointCloud<PointT>() );
        typename pcl::PointCloud<PointT>::ConstPtr model_cloud = m.getAssembled( param_.resolution_mm_model_assembly_ );
        pcl::transformPointCloud( *model_cloud, *model_aligned, transforms_[i]);
        vis_->addPointCloud(model_aligned, model_label.str(), vp3_);
    }
    vis_->setBackgroundColor(1.f, 1.f, 1.f, vp3_);
    vis_->spin();
}


template<>
void
V4R_EXPORTS
Recognizer<pcl::PointXYZRGB>::visualize() const
{
    typedef pcl::PointXYZRGB PointT;

    if(!vis_) {
        vis_.reset(new pcl::visualization::PCLVisualizer("single-view recognition results"));
        vis_->createViewPort(0,0,1,0.33,vp1_);
        vis_->createViewPort(0,0.33,1,0.66,vp2_);
        vis_->createViewPort(0,0.66,1,1,vp3_);
        vis_->addText("input cloud", 10, 10, 20, 1, 1, 1, "input", vp1_);
        vis_->addText("generated hypotheses", 10, 10, 20, 0, 0, 0, "generated hypotheses", vp2_);
        vis_->addText("verified hypotheses", 10, 10, 20, 0, 0, 0, "verified hypotheses", vp3_);
    }

    vis_->removeAllPointClouds();
    vis_->removeAllPointClouds(vp1_);
    vis_->removeAllPointClouds(vp2_);
    vis_->removeAllPointClouds(vp3_);

    Eigen::Vector4f zero_origin; zero_origin[0] = zero_origin[1] = zero_origin[2] = zero_origin[3] = 0.f;
    typename pcl::PointCloud<pcl::PointXYZRGB>::Ptr vis_cloud (new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::copyPointCloud(*scene_, *vis_cloud);
    vis_cloud->sensor_origin_ = zero_origin;
    vis_cloud->sensor_orientation_ = Eigen::Quaternionf::Identity();
    vis_->addPointCloud(vis_cloud, "input cloud", vp1_);
    vis_->setBackgroundColor(.0f, .0f, .0f, vp2_);

    for(size_t i=0; i<models_.size(); i++)
    {
        ModelT &m = *models_[i];
        const std::string model_id = m.id_.substr(0, m.id_.length() - 4);
        std::stringstream model_label;
        model_label << model_id << "_" << i;
        typename pcl::PointCloud<PointT>::Ptr model_aligned ( new pcl::PointCloud<PointT>() );
        typename pcl::PointCloud<PointT>::ConstPtr model_cloud = m.getAssembled( param_.resolution_mm_model_assembly_ );
        pcl::transformPointCloud( *model_cloud, *model_aligned, transforms_[i]);
        vis_->addPointCloud(model_aligned, model_label.str(), vp2_);
    }
    vis_->setBackgroundColor(.5f, .5f, .5f, vp2_);

    for(size_t i=0; i<models_.size(); i++)
    {
        if(i >= hypothesis_is_verified_.size() || !hypothesis_is_verified_[i])
            continue;

        ModelT &m = *models_[i];
        const std::string model_id = m.id_.substr(0, m.id_.length() - 4);
        std::stringstream model_label;
        model_label << model_id << "_v_" << i;
        typename pcl::PointCloud<PointT>::Ptr model_aligned ( new pcl::PointCloud<PointT>() );
        typename pcl::PointCloud<PointT>::ConstPtr model_cloud = m.getAssembled( param_.resolution_mm_model_assembly_ );
        pcl::transformPointCloud( *model_cloud, *model_aligned, transforms_[i]);
        vis_->addPointCloud(model_aligned, model_label.str(), vp3_);
    }

    vis_->setBackgroundColor(1.f, 1.f, 1.f, vp3_);
    vis_->resetCamera();
    vis_->spin();
}

//template<typename PointT>
//void
//Recognizer<PointT>::visualizePlanes() const
//{
//    Eigen::Vector4f zero_origin; zero_origin[0] = zero_origin[1] = zero_origin[2] = zero_origin[3] = 0.f;
//    for(size_t plane_id=0; plane_id < verified_planes_.size(); plane_id++)
//    {
//        std::stringstream plane_name;
//        plane_name << "plane_" << plane_id;
//        verified_planes_[plane_id]->sensor_origin_ = zero_origin;
//        verified_planes_[plane_id]->sensor_orientation_ = Eigen::Quaternionf::Identity();
//        vis_->addPointCloud<PointT> ( verified_planes_[plane_id], plane_name.str (), vp3_ );
//    }
//}

//template <>
//V4R_EXPORTS void
//Recognizer<pcl::PointXYZRGB>::visualizePlanes() const
//{
//    Eigen::Vector4f zero_origin; zero_origin[0] = zero_origin[1] = zero_origin[2] = zero_origin[3] = 0.f;
//    for(size_t plane_id=0; plane_id < verified_planes_.size(); plane_id++)
//    {
//        std::stringstream plane_name;
//        plane_name << "plane_" << plane_id;
//        verified_planes_[plane_id]->sensor_origin_ = zero_origin;
//        verified_planes_[plane_id]->sensor_orientation_ = Eigen::Quaternionf::Identity();
//        pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb_handler ( verified_planes_[plane_id] );
//        vis_->addPointCloud<pcl::PointXYZRGB> ( verified_planes_[plane_id], rgb_handler, plane_name.str (), vp3_ );
//    }
//}


}
