#include <limits>
#include "AbcUtil.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::stringstream;
using std::ifstream;
using std::pow;
using std::pair;
using std::make_pair;


vector<string> split(const string &s, char delim) {
    vector<string> tokens;
    stringstream ss(s);
    string token;
    while (getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}


string slurp(string filename) {
    ifstream ifs(filename.c_str());

    stringstream sstr;
    sstr << ifs.rdbuf();
    return sstr.str();
}


Mat2D read_matrix_file(string filename, char sep) {
    cerr << "Loading " << filename << endl;
    ifstream myfile(filename.c_str());

    vector<vector<double> > M;
    if (myfile.is_open()) {
        string line;

        while ( getline(myfile,line) ) {
            //split string based on "," and store results into vector
            vector<string> fields = split(line, sep);

            vector<double>row(fields.size());
            for( unsigned int i=0; i < fields.size(); i++ ) {
                row[i] = string2double(fields[i]);
            }
            M.push_back(row);
        }
    }

    Mat2D X( (int) M.size(), (int) M[0].size() );
    for( unsigned int i=0; i < M.size(); i++ ) {
        for( unsigned int j=0; j < M[i].size(); j++ ) {  
            X(i,j)=M[i][j]; 
        }
    }
    return X;
}


Row col_stdev( Mat2D mat, Row means ) {
    Row stdevs = Row::Zero(mat.cols());
    const float_type N = mat.rows();
    if ( N < 2 ) return stdevs;

    const float_type N_inv = 1.0/(N-1); // N-1 for unbiased sample variance
    for (int i=0; i<mat.cols(); i++) {
        stdevs[i] = sqrt( (mat.col(i).array()-means[i]).square().sum() * N_inv );
    }
    return stdevs;
}


float_type dominant_eigenvalue( EigenSolver<Mat2Dc> es ){
    Colc  ev = es.eigenvalues();
    float_type m = 0;

    for (int i = 0; i<ev.size(); i++) {
        if (imag(ev[i]) == 0) {
            if (abs(ev[i]) > m) m = abs(ev[i]);
        }
    }
    return m;
}


Colc dominant_eigenvector( EigenSolver<Mat2D> es ){
    Colc eig_val = es.eigenvalues();
    float_type m = 0;
    int idx = 0;

    for (int i = 0; i<eig_val.size(); i++) {
        if (imag(eig_val[i]) == 0) {
            if (abs(eig_val[i]) > m) {
                m = abs(eig_val[i]);
                idx = i;
            }
        }
    }
    return es.eigenvectors().col(idx);
}

Mat2D colwise_z_scores( const Mat2D& mat) {
    Row means, stdev;
    return colwise_z_scores(mat, means, stdev);
}

Mat2D colwise_z_scores( const Mat2D& mat, Row& means, Row& stdev ) {
    // Standardize values by column, i.e. convert to Z-scores
    means = col_means( mat );
    stdev = col_stdev( mat, means );

    // This is not technically correct, since z scores are undefined if the stdev is 0.  
    // In our case, however, the resulting scores cannot be nan, or downstream calculations 
    // are fouled up, so we basically setting them to 0 as a stop-gap solution.  A better solution
    // would be to not pass this parameter in to the PLS regression.  This should be possible
    // but needs to be implemented with care.
    for (int c = 0; c<stdev.size(); c++) if (stdev[c] == 0) stdev[c] = 1;

    Mat2D zmat = Mat2D::Zero(mat.rows(), mat.cols());
    for (int r = 0; r<mat.rows(); r++) { zmat.row(r) = (mat.row(r) - means).cwiseQuotient(stdev); }
    return zmat;
}

/*
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

// -- OR --
template <typename T> inline constexpr
int signum(T x, std::false_type is_signed) {
    return T(0) < x;
}

template <typename T> inline constexpr
int signum(T x, std::true_type is_signed) {
    return (T(0) < x) - (x < T(0));
}

template <typename T> inline constexpr
int signum(T x) {
    return signum(x, std::is_signed<T>());
}*/

/*template <typename T>
vector<size_t> ordered(vector<T> const& values) {
    vector<size_t> indices(values.size());
    std::iota(begin(indices), end(indices), static_cast<size_t>(0));

    std::sort(
        begin(indices), end(indices),
        [&](size_t a, size_t b) { return values[a] < values[b]; }
    );
    return indices;
}*/


// Take a list of values, return original indices sorted by value
vector<int> ordered(Col const& values) {

    vector<pair<float_type,int> > pairs(values.size()); 
    for(unsigned int pos=0; pos<values.size(); pos++) {
        pairs[pos] = make_pair(values[pos],pos);
    }

    //bool comparator ( const mypair& l, const mypair& r) { return l.first < r.first; }

    std::sort( pairs.begin(), pairs.end());
    vector<int> indices(values.size());
    for(unsigned int i=0; i < pairs.size(); i++) indices[i] = pairs[i].second;

    return indices;
}


//
// WILCOXON SIGNED RANK TEST FOR EVALUATING RELATIVE QUALITY OF TWO
// COMPETING METHODS
//
// DESCRIPTION: Pairwise comparison between sets of model predictions
// Competing models: model#1, model#2
//
// REFERENCE: Lehmann E. L. Nonparamtrics: Statistical Methods Based on Ranks.
// Holden-Day: San Francisco, 1975, 120-132.
//
// Let: U=sum of postive ranks, V=sum of negative ranks
// (V>U) is evidence that the model#1 is better)
// Define: d=U-V and t=U+V=n(n+1)/2
// Then V=(t-d)/2
//
// Asymptotic Theory: Suppose n is the number of samples.
// Then, E(V)=n(n+1)/4 and Var(V)=n(n+1)(2n+1)/24.
// It follows that (V-E(V))/Std(V) is approx. normally distributed.
//
// INPUT: err_1=prediction errors from model#1
//        err_2=prediction errors from model#2
//
// OUTPUT: probw=Prob{V is larger than observed}
// If probw is small enough, conclude that model#1 is better
//
// Based on Matlab code from
// Thomas E. V. Non-parametric statistical methods for multivariate calibration 
// model selection and comparison. J. Chemometrics 2003; 17: 653–659
//
float_type wilcoxon(const Col err_1, const Col err_2) {
    int n = err_1.rows();
    Col del = err_1.cwiseAbs() - err_2.cwiseAbs();
    Rowi sdel;
    sdel.setZero(del.size());
    //Matrix<int, Dynamic, 1> sdel = del.unaryExpr(std::ptr_fun(_sgn)); // can't get this to work
    for (int i=0; i<del.size(); i++)  sdel(i) = (0 < del(i)) - (del(i) < 0); // get the sign of each element
    Col adel = del.cwiseAbs();
    // 's' gives the original positions (indices) of the sorted values
    vector<int> s = ordered(adel); 
    float d = 0;
    for (int i=0; i<n; i++) d += (i+1)*sdel(s[i]);
    float t  = n*(n+1)/2.0;
    float v  = (t-d)/2.0;
    float ev = t/2.0;
    double sv = sqrt((double) n*(n+1)*(2*n+1)/24.0);
    float_type z = (v-ev)/sv;
    float_type probw = 1.0 - normalcdf(z);

    return probw;
}

//
// Numerical Approximation to Normal Cumulative Distribution Function
//
// DESCRIPTION:
// REFERENCE: Handbook of Mathematical Functions with Formulas, Graphs, and Mathematical
// Tables, U.S. Dept of Commerce - National Bureau of Standards, Editors: M. Abramowitz and I. A. Stegun
// December 1972, p. 932
// INPUT: z=computed Z-value
// OUTPUT: probn=cumulative probability from -infinity to z
//
//
float_type normalcdf(float_type z){
    const double c1 = 0.196854;
    const double c2 = 0.115194;
    const double c3 = 0.000344;
    const double c4 = 0.019527;
    float_type p;
    if (z < 0) {
        z = -z;
        p = 1 - 0.5 / pow(1 + c1*z + c2*z*z + c3*z*z*z + c4*z*z*z*z,4);
    } else {
        p = 0.5 / pow(1 + c1*z + c2*z*z + c3*z*z*z + c4*z*z*z*z,4);
    }
    float_type probn = 1.0 - p;
    return probn;
}

/*
double normal_pdf(double x, double mu, double var) {
    long double PI = 3.1415926535897932384;
    return exp(-pow(x-mu,2) / (2.0*var)) / sqrt(2*PI*var);
}

double normal_cdf(double x, double mu, double var) {
    x = (x-mu)/sqrt(var);
    // Abramowitz & Stegun (1964) approximation
    long double b0 = 0.2316419;
    double b1 = 0.319381530;
    double b2 = -0.356563782;
    double b3 = 1.781477937;
    double b4 = -1.821255978;
    double b5 = 1.330274429;
    if (x >= 0.0) {
        long double t = 1.0/(1.0+b0*x);
        return 1.0 - normal_pdf(x, 0, 1)*(b1*t + b2*pow(t,2) + b3*pow(t,3) + b4*pow(t,4) + b5*pow(t,5));
    } else {
        long double t = 1.0/(1.0-b0*x);
        return normal_pdf(x, 0, 1)*(b1*t + b2*pow(t,2) + b3*pow(t,3) + b4*pow(t,4) + b5*pow(t,5));
    }
}*/

float_type mean(const Col data) {
    assert( data.size() > 0 );
    return data.sum() / data.size();
}

float_type median(const Col data) {
    assert(data.size() > 0);
    // copy & sort data
    vector<float_type> vdata(data.data(), data.data()+data.size());
    const int n = vdata.size();
    sort(vdata.begin(), vdata.end());

    float_type median;

    if (n % 2 == 0) {
        median = (vdata[n / 2 - 1] + vdata[n / 2]) / 2;
    } else {
        median = vdata[n / 2];
    }

    return median;
}

float_type variance(const Col data, float_type _mean) {
    if (data.size() < 2) { 
        cerr << "WARNING: Variance called with " << data.size() << " data values. Returning 0." << endl;
        return 0;
    } else {
        return (data.array() - _mean).square().sum() / (data.size() - 1);
    }
}

float_type max(const Col data) {
    assert(data.size() > 0);
    float_type m = data[0];
    for (int i = 1; i< data.size(); i++) {
        const float_type v = data[i];
        if (v > m) m = v;
    }
    return m;
}

float_type skewness(const Col data) {
    float_type _x = mean(data);
    float_type _v = variance(data, _x);
    return ((data.array() - _x).pow(3).sum() / data.size() ) / pow(_v, 1.5);
}

int _mc_pos(const float_type v, const float_type m) {
    enum Position {ABOVE, BELOW, AT};
    Position p;
    if (v > m) { p = ABOVE; }
    else if (v < m) { p = BELOW; }
    else { p = AT; }
    return (int) p;
}

float_type median_crossings(const Col data) {
    if (data.size() < 2) {
        return 0;
    } else {
        return median_crossings(data, median(data));
    }
}

// Calculates the number of times the data series crosses the median
float_type median_crossings(const Col data, const float_type m) {
    int mc = 0;
    if (data.size() < 2) return mc;

    enum Position {ABOVE, BELOW, AT};
    // current and next are like cursors that trace the data series
    Position current, next;
    current = (Position) _mc_pos(data[0], m);
    if (current == AT) mc++; // increment if we're starting at the median
    for (int i = 1; i < data.size(); ++i) {
        next = (Position) _mc_pos(data[i], m);
        if (next != current and current != AT) mc++; // just crossed or touched the median
        current = next;
    }
    return ((float_type) mc)/(data.size()-1);
}

float optimize_box_cox (const Col data, float lambda_min, float lambda_max, float step) {
    float best_lambda = lambda_min;
    float min_skew = std::numeric_limits<float>::infinity();
    float skew;
    for (float lambda = lambda_min; lambda <= lambda_max; lambda += step) {
        if (lambda == 0) {
            skew = skewness( data.array().log() );
        } else {
            skew = skewness( (data.array().pow(lambda) - 1).matrix() / lambda );
        }
        if (abs(skew) < abs(min_skew)) {
            min_skew = skew;
            best_lambda = lambda;
        }
    }
    return best_lambda;
}

float optimize_box_cox (const Col data) {
    return optimize_box_cox(data, -5, 5, 0.1);
}

std::string exec(std::string cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "ERROR: Unable to create pipe to " << cmd << endl;
        return "ERROR";
    }
    char buffer[512];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 512, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}

int gsl_rng_nonuniform_int(vector<double>& weights, const gsl_rng* rng) {
    // Weights must sum to 1!!
    double running_sum = 0;
    double r = gsl_rng_uniform(rng);
    for (unsigned int i = 0; i<weights.size(); i++) {
        running_sum += weights[i];
        if (r<=running_sum) return i;
    }
    cerr << "ERROR: Weights may not be normalized\n\t Weights summed to: " << running_sum << endl;
    exit(100);
}

double rand_trunc_normal(double mu, double sigma_squared, double min, double max, const gsl_rng* rng) {
    double sigma = sqrt(sigma_squared);
    // Don't like this, but it will work
    // as long as min and max are reasonable
    // (relative to the pdf)
    while (1) {
        double dev = gsl_ran_gaussian(rng, sigma) + mu; 
        if (dev >= min and dev <= max) {
            return dev;
        }
    }
}
 
Fit* lin_reg(const std::vector<double> &x, const std::vector<double> &y) {
    assert( x.size() == y.size() );
    Fit* fit = new Fit();
    const int n = x.size();
    double sumx = 0.0;                        /* sum of x                      */
    double sumx2 = 0.0;                       /* sum of x**2                   */
    double sumxy = 0.0;                       /* sum of x * y                  */
    double sumy = 0.0;                        /* sum of y                      */
    double sumy2 = 0.0;                       /* sum of y**2                   */

    for (int i=0; i<n; i++)   { 
        sumx  += x[i];       
        sumx2 += pow(x[i],2);  
        sumxy += x[i] * y[i];
        sumy  += y[i];      
        sumy2 += pow(y[i],2); 
    } 

    double denom = n * sumx2 - pow(sumx,2);
    if (denom == 0) {
        // singular matrix. can't solve the problem.
        fit->m   = 0;
        fit->b   = 0;
        fit->rsq = 0;
        return fit;
    }

    fit->m = (n * sumxy  -  sumx * sumy) / denom;
    fit->b = (sumy * sumx2  -  sumx * sumxy) / denom;
    // compute correlation coeff
    fit->rsq = pow((sumxy - sumx * sumy / n) / sqrt((sumx2 - pow(sumx,2)/n) * (sumy2 - pow(sumy,2)/n)),2);

    return fit; 
}
