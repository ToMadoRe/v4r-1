/******************************************************************************
 * Copyright (c) 2017 Thomas Faeulhammer
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

#pragma once
#include <v4r/segmentation/plane_extractor.h>

namespace v4r
{

template<typename PointT>
class V4R_EXPORTS OrganizedMultiPlaneExtractor : public PlaneExtractor<PointT>
{
protected:
    using PlaneExtractor<PointT>::cloud_;
    using PlaneExtractor<PointT>::normal_cloud_;
    using PlaneExtractor<PointT>::all_planes_;
    using PlaneExtractor<PointT>::param_;

public:
    OrganizedMultiPlaneExtractor( const PlaneExtractorParameter &p = PlaneExtractorParameter() ) :
        PlaneExtractor<PointT>(p)
    {}

    virtual void compute();
    virtual bool getRequiresNormals() const { return true; }

    typedef boost::shared_ptr< OrganizedMultiPlaneExtractor<PointT> > Ptr;
    typedef boost::shared_ptr< OrganizedMultiPlaneExtractor<PointT> const> ConstPtr;
};

}
