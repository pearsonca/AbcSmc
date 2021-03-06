#ifndef ABCUTIL_H
#define ABCUTIL_H

#include <iostream>
#include <sstream>
#include <vector>
#include <assert.h>
#include <iomanip>
#include <fstream>
#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include <math.h>

using namespace std;

class Fit {
    public:
        double m;
        double b;
        double rsq;
};

#ifdef USING_MPI
#include <mpi.h>
struct MPI_par {
    MPI_Comm comm;
    MPI_Info info;
    int mpi_size, mpi_rank;
};
#else
struct MPI_par {
    const static int mpi_size = 1;
    const static int mpi_rank = 0;
};
#endif

//using namespace std;
using namespace Eigen;

#ifdef MPREAL_SUPPORT
#include "mpreal.h"
#include <unsupported/Eigen/MPRealSupport>
    using namespace mpfr;
    typedef mpreal float_type;
#else
    typedef long double float_type;
#endif 

typedef Matrix<float_type,Dynamic,Dynamic> Mat2D;
typedef Matrix<float_type, Dynamic, 1>  Col;
typedef Matrix<float_type, 1, Dynamic>  Row;
typedef Matrix<int, 1, Dynamic>  Rowi;
typedef Matrix<std::complex<float_type>,Dynamic,Dynamic> Mat2Dc;
typedef Matrix<std::complex<float_type>, Dynamic, 1>  Colc;

vector<string> split(const std::string& s, char c);

inline int string2int(const std::string& s){ std::istringstream i(s); int x = 0; i >> x; return x; }
inline double string2double(const std::string& s){ std::istringstream i(s); double x = 0; i >> x; return x; }
inline float_type string2float_type(const std::string& s){ std::istringstream i(s); float_type x = 0; i >> x; return x; }

std::string slurp(std::string filename);

inline Row col_means( Mat2D mat ) { return mat.colwise().sum() / mat.rows(); }

//int _sgn(float_type val) { return (0 < val) - (val < 0); }

Mat2D read_matrix_file(std::string filename, char sep); 

Row col_stdev( Mat2D mat, Row means );

float_type dominant_eigenvalue( EigenSolver<Mat2Dc> es );

Colc dominant_eigenvector( EigenSolver<Mat2D> es );

Mat2D colwise_z_scores( const Mat2D& mat );
Mat2D colwise_z_scores( const Mat2D& mat, Row& means, Row& stdev );

std::vector<int> ordered(Col const& values);

float_type wilcoxon(const Col err_1, const Col err_2);

float_type normalcdf(float_type z);

float_type mean(const Col data);

float_type median(const Col data);

float_type variance(const Col data, float_type _mean);

float_type max(const Col data);

float_type skewness(const Col data);

float_type median_crossings(const Col data, const float_type _median);

float_type median_crossings(const Col data);

float optimize_box_cox (const Col data, float lambda_min, float lambda_max, float step);

float optimize_box_cox (const Col data);

std::string exec(std::string cmd);

template <typename T>
inline void cerr_vector(std::vector<T> & my_vector, std::string sep = " ") {
    for (int i = 0; i < my_vector.size() - 1; i++ ) std::cerr << my_vector[i] << sep;
    std::cerr << my_vector.back();
}

template <typename T>
inline void cout_vector(std::vector<T> & my_vector, std::string sep = " ") {
    for (int i = 0; i < my_vector.size() - 1; i++ ) std::cout << my_vector[i] << sep;
    std::cout << my_vector.back();
}


template <typename T>
inline std::string toString (const T& t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
}

inline double uniform_pdf(double a, double b) { return 1.0 / fabs(b-a); }

int gsl_rng_nonuniform_int(std::vector<double>& weights, const gsl_rng* rng);

double rand_trunc_normal(double mu, double sigma_squared, double min, double max, const gsl_rng* rng); 
 
Fit* lin_reg(const std::vector<double> &x, const std::vector<double> &y);

inline vector<float_type> as_vector(const Row data) {
    vector<float_type> vec(data.size());
    for (int i = 0; i < data.size(); i++) vec[i] = data[i];
    return vec;
}

inline Row as_row(const vector<float_type> data) {
    Row row(data.size());
    for (unsigned int i = 0; i < data.size(); i++) row[i] = data[i];
    return row;
}

#endif
