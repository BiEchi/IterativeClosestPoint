//=============================================================================
//
//   Code framework for the lecture
//
//   "Surface Representation and Geometric Modeling"
//
//   Mark Pauly, Mario Botsch, Balint Miklos, and Hao Li
//
//   Copyright (C) 2007 by  Applied Geometry Group and
//                          Computer Graphics Laboratory, ETH Zurich
//
//-----------------------------------------------------------------------------
//
//                                License
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.
//
//=============================================================================
//=============================================================================
//
//  CLASS RegistrationViewer - IMPLEMENTATION
//
//=============================================================================


//== INCLUDES =================================================================

#include <OpenMesh/Core/IO/MeshIO.hh>
#include "RegistrationViewer.hh"
#include "Registration.hh"
#include "ClosestPoint.hh"
#include "gl.hh"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unordered_set>
#include <algorithm>

#define PI 3.14159265

//== IMPLEMENTATION ==========================================================
template <typename Elem>
class randomaccesstable
{
public:
	randomaccesstable(std::size_t initial_size)
		: data_(initial_size), count_(0)
	{ }

	inline randomaccesstable &push_back(const Elem &elem)
	{
		if (count_ < data_.size())
			data_[count_++] = elem;
		else {
			data_.push_back(elem);
			++count_;
		}
		return *this;
	}

	inline randomaccesstable &remove(const std::size_t index)
	{
		if (index < count_)
		{
			std::swap(data_[index], data_[count_ - 1]);
			--count_;
		}
		return *this;
	}

	inline const Elem &operator[](const std::size_t index) const
	{
		return data_[index];
	}

	inline Elem &operator[](const std::size_t index)
	{
		return data_[index];
	}

	inline std::size_t size() const
	{
		return count_;
	}

public:
	std::vector<Elem>  data_;
	std::size_t        count_;
};

RegistrationViewer::
RegistrationViewer(const char* _title, int _width, int _height)
  : GlutExaminer(_title, _width, _height)
{
    clear_draw_modes();

    currIndex_ = 0;
    numProcessed_ = 0;

    mode_ = VIEW;
}


//-----------------------------------------------------------------------------


RegistrationViewer::
~RegistrationViewer()
{
}

//-----------------------------------------------------------------------------


/// set output filename
void
RegistrationViewer::
set_output( const std::string & filename )
{
    outputFilename_ = filename;
}

 //-----------------------------------------------------------------------------

bool RegistrationViewer::open_meshes(const std::vector<std::string> & _filenames)
{
    bool success = true;

    for(int i = 0; i < (int) _filenames.size(); i++)
    {
        Mesh mesh;
        mesh.request_vertex_status();
        mesh.request_edge_status();
        mesh.request_face_status();
        mesh.request_face_normals();
        mesh.request_vertex_normals();

        // load mesh
        if (OpenMesh::IO::read_mesh(mesh, _filenames[i].c_str()))
        {
            // clean mesh
            clean_mesh( mesh );

            Mesh::ConstVertexIter  v_begin(mesh.vertices_begin()),
                                   v_end(mesh.vertices_end()),
                                   v_it;
            Mesh::Point            gravity(0,0,0);

            // calculate center of gravity
            for (v_it = v_begin; v_it!=v_end; ++v_it)
            {
                gravity += mesh.point(v_it);
            }

            gravity /= float( mesh.n_vertices() );

            // move to center of gravity
            for (v_it = v_begin; v_it!=v_end; ++v_it)
            {
                mesh.point(v_it) -= gravity;
            }

            // compute face & vertex normals
            mesh.update_normals();

            // compute average vertex distance
            averageVertexDistance_ = get_average_vertex_distance( mesh );

            // info
            std::cerr << _filenames[i] << ": " << mesh.n_vertices() << " vertices, " << mesh.n_faces()    << " faces\n";
        }
        else
        {
            success = false;
        }

        meshes_.push_back( mesh );
        transformations_.push_back( Transformation() );
    }


    // calculate bounding box of all points
    Mesh::Point            gravity(0,0,0);
    float count = 0;
    Mesh::Point            bbMin(1e9,1e9,1e9), bbMax(-1e9,-1e9,-1e9);
    for(int i = 0; i < (int) meshes_.size(); i++)
    {
        Mesh::ConstVertexIter   v_it(meshes_[i].vertices_begin()),
                                v_end(meshes_[i].vertices_end());

        for (; v_it!=v_end; ++v_it)
        {
            bbMin.minimize(meshes_[i].point(v_it));
            bbMax.maximize(meshes_[i].point(v_it));
            gravity[0] += meshes_[i].point(v_it)[0];
            gravity[1] += meshes_[i].point(v_it)[1];
            gravity[2] += meshes_[i].point(v_it)[2];
            count++;
        }
    }

    set_scene( Vec3f(0,0,0), 0.3*(bbMin - bbMax).norm());

    if( success )
    {
        // update face indices for faster rendering
        update_face_indices();

        glutPostRedisplay();
    }

    numProcessed_ = std::min( 2, int(meshes_.size()) );
    currIndex_ = std::max( 0, numProcessed_-1 );

    return success;
}


//-----------------------------------------------------------------------------


void RegistrationViewer::update_face_indices()
{
    indices_.clear();

    for(int i = 0; i < (int) meshes_.size(); i++)
    {
        indices_.push_back( std::vector<unsigned int>() );

        Mesh::ConstFaceIter        f_it(meshes_[i].faces_sbegin()),
                                   f_end(meshes_[i].faces_end());
        Mesh::ConstFaceVertexIter  fv_it;

        indices_[i].clear();
        indices_[i].reserve(meshes_[i].n_faces()*3);

        for (; f_it!=f_end; ++f_it)
        {
            indices_[i].push_back((fv_it=meshes_[i].cfv_iter(f_it)).handle().idx());
            indices_[i].push_back((++fv_it).handle().idx());
            indices_[i].push_back((++fv_it).handle().idx());
        }
    }
}


//-----------------------------------------------------------------------------


void RegistrationViewer::draw(const std::string& _draw_mode)
{
    if (indices_.empty())
    {
        GlutExaminer::draw(_draw_mode);
        return;
    }

    // display scans
    for(int i = 0; i < numProcessed_; i++)
    {
        if( i == currIndex_ )
            draw(i, OpenMesh::Vec3f(0.1,0.5,0.1) );
        else
            draw(i, OpenMesh::Vec3f(0.5,0.5,0.5) );
    }

    // display subsampled points
    std::vector< Vector3d > pts = get_points( meshes_[currIndex_] );
    glEnable(GL_COLOR_MATERIAL);
    glColor3f(0,0,1);
    for(int i = 0; i < (int) sampledPoints_.size(); i++)
    {
        glPushMatrix();
        Vector3d pt = pts[sampledPoints_[i]];
        pt = transformations_[currIndex_].transformPoint( pt );
        glTranslatef( pt[0], pt[1], pt[2] );
        glutSolidSphere( averageVertexDistance_, 10, 10 );
        glPopMatrix();
    }
    glDisable(GL_COLOR_MATERIAL);
}


//-----------------------------------------------------------------------------


void RegistrationViewer::draw(int index, const OpenMesh::Vec3f & color)
{
    glPushMatrix();
    // apply transformation matrix of scan
    transformations_[index].apply_gl();

    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glColor3f(color[0], color[1], color[2]);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    GL::glVertexPointer(meshes_[index].points());
    GL::glNormalPointer(meshes_[index].vertex_normals());

    glDrawElements(GL_TRIANGLES, indices_[index].size(), GL_UNSIGNED_INT, &(indices_[index][0]));

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_COLOR_MATERIAL);

    glPopMatrix();
}



//-----------------------------------------------------------------------------

void RegistrationViewer::keyboard(int key, int x, int y)
{
    switch (key)
    {
        case ' ':
        {
            std::cout << "Register point-2-surface..." << std::endl;
            perform_registration(true);
            glutPostRedisplay();
            break;
        }
        case 'r':
        {
            std::cout << "Register point-2-point..." << std::endl;
            perform_registration(false);
            glutPostRedisplay();
            break;
        }
        case 'n':
        {
            sampledPoints_.clear();
            numProcessed_ = std::min( numProcessed_+1, int(meshes_.size()) );
            currIndex_ = (currIndex_+1) % int(meshes_.size());
            std::cout << "Process scan " << currIndex_ << " of " << int(meshes_.size()) << std::endl;
            glutPostRedisplay();
            break;
        }
        case 's':
        {
            save_points();
            break;
        }
        case 'h':
        {
            printf("Help:\n");
            printf("SHIFT and move mouse: manual alignment\n");
            printf("'h'\t-\thelp\n");
            printf("'n'\t-\tnext mesh\n");
            printf("'r'\t-\tregister current mesh selected mesh using point-2-point optimization\n");
            printf("' '\t-\tregister current mesh selected mesh using point-2-surface optimization\n");
            printf("'s'\t-\tsave points to output\n");
            break;
        }
        default:
        {
            GlutExaminer::keyboard(key, x, y);
            break;
        }
    }
}



//-----------------------------------------------------------------------------

void RegistrationViewer::mouse(int button, int state, int x, int y)
{
    // manual object transformation when pressing SHIFT
    if( glutGetModifiers() & GLUT_ACTIVE_SHIFT ) mode_ = MOVE;
    else mode_ = VIEW;

    GlutExaminer::mouse(button, state, x, y);
}





//-----------------------------------------------------------------------------

/// called during mouse motion while button is pressed
void RegistrationViewer::motion(int x, int y)
{
    switch (mode_)
    {
        default:
        case VIEW:
        {
            GlutExaminer::motion(x, y);
            break;
        }
        case MOVE:
        {
            // manual object transformation when pressing SHIFT

            // zoom
            if (button_down_[0] && button_down_[1])
            {
                float dy = y - last_point_2D_[1];
                float h  = height_;
                Transformation mv_tr = Transformation::retrieve_gl();
                Transformation tr(0.0, 0.0, radius_ * dy * 3.0 / h);
                transformations_[currIndex_] =  mv_tr.inverse() * tr * mv_tr * transformations_[currIndex_];
            }
            // rotation
            else if (button_down_[0])
            {
                if (last_point_ok_)
                {
                    Vec2i  new_point_2D;
                    Vec3f  new_point_3D;
                    bool   new_point_ok;

                    new_point_2D = Vec2i(x, y);
                    new_point_ok = map_to_sphere(new_point_2D, new_point_3D);

                    if (new_point_ok)
                    {
                        Vec3f axis      = (last_point_3D_ % new_point_3D);
                        float cos_angle = (last_point_3D_ | new_point_3D);

                        if (fabs(cos_angle) < 1.0)
                        {
                            float angle = 2.0*acos(cos_angle);
                            //rotate(axis, angle);
                            Transformation mv_tr = Transformation::retrieve_gl();
                            mv_tr.translation_.fill(0);
                            Transformation tr(angle, Vector3f(axis[0],axis[1],axis[2]));

                            transformations_[currIndex_] = mv_tr.inverse() * tr * mv_tr * transformations_[currIndex_];
                        }
                    }
                }
            }
            // translation
            else if (button_down_[1])
            {
                float dx = x - last_point_2D_[0];
                float dy = y - last_point_2D_[1];

                float z = - ((modelview_matrix_[ 2]*center_[0] +
                              modelview_matrix_[ 6]*center_[1] +
                              modelview_matrix_[10]*center_[2] +
                              modelview_matrix_[14]) /
                             (modelview_matrix_[ 3]*center_[0] +
                              modelview_matrix_[ 7]*center_[1] +
                              modelview_matrix_[11]*center_[2] +
                              modelview_matrix_[15]));

                float aspect = (float)width_ / (float)height_;
                float up     = tan(fovy_/2.0f*M_PI/180.f) * near_;
                float right  = aspect*up;

                Transformation mv_tr = Transformation::retrieve_gl();
                Transformation tr(2.0*dx/width_*right/near_*z, -2.0*dy/height_*up/near_*z, 0.0f);
                transformations_[currIndex_] = mv_tr.inverse() * tr * mv_tr * transformations_[currIndex_];
            }


            // remeber points
            last_point_2D_ = Vec2i(x, y);
            last_point_ok_ = map_to_sphere(last_point_2D_, last_point_3D_);

            glutPostRedisplay();
            break;
        }
    }
}


//=============================================================================


/// save current points
void RegistrationViewer::save_points()
{
    std::ofstream out( outputFilename_.c_str() );
    for(int i = 0; i < numProcessed_; i++)
    {
        // get points on target meshes
        std::vector< Vector3d > pts = get_points( meshes_[i] );
        std::vector< Vector3d > normals = get_normals( meshes_[i] );

        // transform using current scan transformations
        pts = transformations_[i].transformPoints( pts );
        normals = transformations_[i].transformVectors( normals );

        for(int j = 0; j < (int) pts.size(); j++)
        {
            out << "v " << pts[j][0] << " " << pts[j][1] << " " << pts[j][2] << " ";
            out << "vn " << normals[j][0] << " " << normals[j][1] << " " << normals[j][2] << std::endl;
        }
    }
    out.close();
    std::cout << "merged points saved to: " << outputFilename_ << std::endl;
}


//=============================================================================

/// clean mesh by removing "bad" triangles
void RegistrationViewer::clean_mesh( Mesh & _mesh )
{
    Mesh::FaceIter f_it = _mesh.faces_begin();
    for(; f_it != _mesh.faces_end(); ++f_it)
    {
        float maxEdge = 0;
        float minEdge = 1e9;
        Mesh::ConstFaceHalfedgeIter fh_it = _mesh.fh_iter( f_it.handle() );
        while(fh_it)
        {
            OpenMesh::Vec3f p = _mesh.point( _mesh.from_vertex_handle( fh_it.handle() ) );
            OpenMesh::Vec3f q = _mesh.point( _mesh.to_vertex_handle( fh_it.handle() ) );
            float edgeLength = sqrt( (p-q) | (p-q) );
            maxEdge = std::max( maxEdge, edgeLength );
            minEdge = std::min( minEdge, edgeLength );

            ++fh_it;
        }
        if( minEdge / maxEdge < 0.2 )
            _mesh.delete_face(f_it.handle(), true);
    }
    _mesh.garbage_collection();
}





//=============================================================================


/// get average vertex distance
float RegistrationViewer::get_average_vertex_distance(const Mesh & _mesh)
{
    float accDist = 0;
    int accCount = 0;

    Mesh::ConstHalfedgeIter he_it = _mesh.halfedges_begin();
    for(; he_it != _mesh.halfedges_end(); ++he_it)
    {
        OpenMesh::Vec3f p = _mesh.point( _mesh.from_vertex_handle( he_it.handle() ) );
        OpenMesh::Vec3f q = _mesh.point( _mesh.to_vertex_handle( he_it.handle() ) );
        float edgeLength = sqrt( (p-q) | (p-q) );
        accDist += edgeLength;
        accCount++;
    }

    if(accCount > 0) return accDist / float(accCount);
    else return 0;
}


//=============================================================================

// get points of mesh
std::vector< Vector3d >
RegistrationViewer::
get_points(const Mesh & _mesh)
{
    std::vector< Vector3d > pts;

    Mesh::ConstVertexIter  v_it(_mesh.vertices_begin()), v_end(_mesh.vertices_end());
    for (; v_it!=v_end; ++v_it)
    {
        Vec3f p = _mesh.point(v_it);
        pts.push_back( Vector3d(p[0], p[1], p[2]) );
    }

    return pts;
}


//=============================================================================

// get normals of mesh
std::vector< Vector3d >
RegistrationViewer::
get_normals(const Mesh & _mesh)
{
    std::vector< Vector3d > normals;

    Mesh::ConstVertexIter  v_it(_mesh.vertices_begin()), v_end(_mesh.vertices_end());
    for (; v_it!=v_end; ++v_it)
    {
        Vec3f n = _mesh.normal(v_it);
        normals.push_back( Vector3d(n[0], n[1], n[2]) );
    }

    return normals;
}


//=============================================================================

// get border vertices of mesh
std::vector< bool >
RegistrationViewer::
get_borders(const Mesh & _mesh)
{
    std::vector< bool > borders;

    Mesh::ConstVertexIter  v_it(_mesh.vertices_begin()), v_end(_mesh.vertices_end());
    for (; v_it!=v_end; ++v_it)
    {
        borders.push_back( _mesh.is_boundary(v_it) );
    }

    return borders;
}



//=============================================================================

/// perform registration
void
RegistrationViewer::
perform_registration(bool _tangential_motion)
{
    std::vector< Vector3d > src;
    std::vector< Vector3d > target;
    std::vector< Vector3d > target_normals;

    // calculate correspondences
    calculate_correspondences( src, target, target_normals );

    Registration reg;
    printf("Num correspondences: %d\n", int(src.size()) );


    // calculate optimal transformation
    Transformation opt_tr;
    if( _tangential_motion )
    {
        opt_tr = reg.register_point2surface( src, target, target_normals );
    }
    else
    {
        opt_tr = reg.register_point2point( src, target );
    }

    // set transformation
    transformations_[currIndex_] = opt_tr * transformations_[currIndex_];
}
static bool sample_valid(const Vector3d &p,const std::vector<int>& ids,const std::vector< Vector3d > & _pts,float len2){
    for(int j = 0; j < ids.size(); ++j){
        if(length2(p - _pts[ids[j]]) < len2)
            return false;
    }
    return true;
}


//=============================================================================
/// subsample points
std::vector<int> RegistrationViewer::subsample( const std::vector< Vector3d > & _pts )
{
    std::vector<int> indeces;
    std::vector<Vector3d> pts_chosen;

    float subsampleRadius = 5 * averageVertexDistance_;

    // EXERCISE 2.2 /////////////////////////////////////////////////////////////
    // subsampling:
    // perform uniform subsampling by storing the indeces of the subsampled points in 'indeces'
    // (Hint: take advantage of the fact that consecutive points in the
    //  vector _pts are also often close in the scan )
    ////////////////////////////////////////////////////////////////////////////

    // set up the first one in all vertices
    int i = 0;
    indeces.push_back(i);
    pts_chosen.push_back(_pts[i]);

    // find the smallest amount of vertices for all input meshes
    int n_vertices_smallest = meshes_[0].n_vertices();
    for (int k = 0; k < meshes_.size(); k++)
    {
        if (meshes_[k].n_vertices() < n_vertices_smallest)
            n_vertices_smallest = meshes_[k].n_vertices();
    }

    // iteration step. for testing use 4, for final examination use 1 or 2
    int step = 4;

    // iterate through all vertices and find the subsampled points
    for (auto iter = _pts.begin(); iter != _pts.end(), i < n_vertices_smallest - step; iter+=step, i+=step)
    {
        bool inside_range = false;
        printf("You're processing point index %d\n", i);

        // optimized: too early vertices will not be needed to examine (using sliding-window approach)
        int window_size = 40;
        auto window_starter = pts_chosen.size() < window_size ?
            pts_chosen.begin() : pts_chosen.begin() + pts_chosen.size() - window_size;
        
        // iterate through to gain the subsampled vertices
        for (auto iter_chosen = window_starter; iter_chosen != pts_chosen.end(); iter_chosen++)
        {
            if (length(*iter - *iter_chosen) < subsampleRadius)
                inside_range = true;
        }
        if (inside_range == false)
        {
            indeces.push_back(i);
            pts_chosen.push_back(_pts[i]);
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    // keep indeces/samples for display
    sampledPoints_ = indeces;
    printf("subsample: choose %d samples\n",indeces.size());
    return indeces;
}


//=============================================================================

/// calculate correspondences
void RegistrationViewer::calculate_correspondences(
    std::vector< Vector3d > & _src,
    std::vector< Vector3d > & _target,
    std::vector< Vector3d > & _target_normals )
{
    _src.clear();
    _target.clear();
    _target_normals.clear();

    std::vector< Vector3d > srcCandidatePts;
    std::vector< Vector3d > srcCandidateNormals;
    std::vector< Vector3d > targetCandidatePts;
    std::vector< Vector3d > targetCandidateNormals;
    std::vector< double > src_target_dis2;

    // get points on src mesh
    std::vector< Vector3d > srcPts = get_points( meshes_[currIndex_] );
    std::vector< Vector3d > srcNormals = get_normals( meshes_[currIndex_] );

    // transform using current scan transformation
    srcPts = transformations_[currIndex_].transformPoints( srcPts );
    srcNormals = transformations_[currIndex_].transformVectors( srcNormals );

    // subsample the points
    std::vector<int> indeces = subsample( srcPts );

    // iterate over all previously processed scans and find correspondences
    // note that we perform registration to all other scans simultaneously, not only pair-wise
    for(int i = 0; i < numProcessed_; i++)
    {
        if( i == currIndex_ ) continue;

        // get points on target meshes
        std::vector< Vector3d > targetPts = get_points( meshes_[i] );
        std::vector< Vector3d > targetNormals = get_normals( meshes_[i] );
        std::vector< bool > targetBorders = get_borders( meshes_[i] );

        // transform using current scan transformation
        targetPts = transformations_[i].transformPoints( targetPts );
        targetNormals = transformations_[i].transformVectors( targetNormals );

        // set up closest point structure
        ClosestPoint cp;
        cp.init( targetPts );

        // find closest points for each src vertex
        for(int j = 0; j < (int) indeces.size(); j++)
        {
            int index = indeces[j];

            int bestIndex = cp.getClosestPoint(srcPts[index]);

            // do not keep border correspondences
            if( !targetBorders[bestIndex] )
            {
                srcCandidatePts.push_back( srcPts[index] );
                srcCandidateNormals.push_back( srcNormals[index] );
                targetCandidatePts.push_back( targetPts[bestIndex] );
                targetCandidateNormals.push_back( targetNormals[bestIndex] );
                src_target_dis2.push_back(length2(srcPts[index]-targetPts[bestIndex]));
            }
        }
    }

    printf("calculate_correspondences: candidate num: %d\n",srcCandidatePts.size());

    // EXERCISE 2.3 /////////////////////////////////////////////////////////////
    // correspondence pruning:
    // prune correspondence based on
    // - distance threshold
    // - normal compatability
    //
    // fill _src, _target, and _target_normals from the candidate pairs

    // normals of correspondences do not deviate more than 60 degrees
    float normalCompatabilityThresh = 60;
    // distance threshold is 3 times the median distance
    float distMedianThresh = 3;

    ////////////////////////////////////////////////////////////////////////////

    // we use a tombstone method for pruning
    int size = srcCandidateNormals.size();
    double tombstone = 0.0;

    // mark the places of not matched ones
    for (int index = 0; index < size; index++)
    {
        // calculate the long edge of the triangle
        Vector3d long_edge = srcCandidateNormals[index].normalize() - targetCandidateNormals[index].normalize();
        double length_long_edge = length(long_edge);

        // compute the normal vector compatibility and distance thresh
        if (src_target_dis2[index] > distMedianThresh ||
            2 * asin(length_long_edge/2) * 180 / PI > normalCompatabilityThresh)
                src_target_dis2[index] = tombstone;
    }

    // delete those ones with tombstones
    int counter = 0;
    for (auto iter = src_target_dis2.begin(); iter != src_target_dis2.end(); /*DO NOTHING HERE*/)
    {
        if (*iter == tombstone)
        {
            srcCandidatePts.erase(srcCandidatePts.begin() + counter);
            srcCandidateNormals.erase(srcCandidateNormals.begin() + counter);
            targetCandidatePts.erase(targetCandidatePts.begin() + counter);
            targetCandidateNormals.erase(targetCandidateNormals.begin() + counter);
            iter = src_target_dis2.erase(src_target_dis2.begin() + counter);
        }
        else 
        {
            iter++;
            counter++;
        }
    }

    _src = srcCandidatePts;
    _target = targetCandidatePts;
    _target_normals = targetCandidateNormals;

    ////////////////////////////////////////////////////////////////////////////

}

//=============================================================================
