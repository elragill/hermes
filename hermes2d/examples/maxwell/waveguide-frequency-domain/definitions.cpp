#include "weakform/weakform.h"
#include "integrals/integrals_hcurl.h"
#include "boundaryconditions/essential_bcs.h"

/* Weak forms */

class CustomMatrixForm : public WeakForm::MatrixFormVol
{
public:
  CustomMatrixForm(int i, int j, double e_0, double mu_0, double mu_r, double kappa, bool align_mesh) 
    : WeakForm::MatrixFormVol(i, j, HERMES_SYM), e_0(e_0), mu_0(mu_0), 
      mu_r(mu_r), kappa(kappa), align_mesh(align_mesh) { }

  template<typename Real, typename Scalar>
  Scalar matrix_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *u, 
                     Func<Real> *v, Geom<Real> *e, ExtData<Scalar> *ext) {
    cplx ikappa = cplx(0.0, kappa);
    return 1.0/mu_r * int_curl_e_curl_f<Real, Scalar>(n, wt, u, v) -
           ikappa * sqrt(mu_0 / e_0) * int_F_e_f<Real, Scalar>(n, wt, gamma, u, v, e) -
           sqr(kappa) * int_F_e_f<Real, Scalar>(n, wt, er, u, v, e);
  }

  scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *u, 
               Func<double> *v, Geom<double> *e, ExtData<scalar> *ext) {
    return matrix_form<scalar, scalar>(n, wt, u_ext, u, v, e, ext);
  }

  Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, 
          Geom<Ord> *e, ExtData<Ord> *ext) {
    return matrix_form<Ord, Ord>(n, wt, u_ext, u, v, e, ext);
  }

  // Gamma as a function of x, y.
  double gamma(int marker, double x, double y)
  {
    if (align_mesh && marker == 1) return 0.03;
    if (!align_mesh && in_load(x,y)) {
      double cx = -0.152994121;  double cy =  0.030598824;
      double r = sqrt(sqr(cx - x) + sqr(cy - y));
      return (0.03 + 1)/2.0 - (0.03 - 1) * atan(10.0*(r -  0.043273273)) / M_PI;
    }
    return 0.0;
  }

  double gamma(int marker, Ord x, Ord y)
  {  
    return 0.0; 
  }

  // Relative permittivity as a function of x, y.
  double er(int marker, double x, double y)
  {
    if (align_mesh && marker == 1) return 7.5;
    if (!align_mesh && in_load(x,y)) {
      double cx = -0.152994121;  double cy =  0.030598824;
      double r = sqrt(sqr(cx - x) + sqr(cy - y));
      return (7.5 + 1)/2.0 - (7.5 - 1) * atan(10.0*(r -  0.043273273)) / M_PI;
    }
    return 1.0;
  }

  double er(int marker, Ord x, Ord y)
  {  
    return 1.0; 
  }

  // Geometry of the load.
  bool in_load(double x, double y) 
  {
    double cx = -0.152994121;
    double cy =  0.030598824;
    double r = 0.043273273;
    if (sqr(cx - x) + sqr(cy - y) < sqr(r)) return true;
    else return false;
  }

  private:
  double e_0, mu_0, mu_r, kappa, align_mesh;
};

class CustomVectorFormSurf : public WeakForm::VectorFormSurf
{
public:
  CustomVectorFormSurf(double omega, double J) 
    : WeakForm::VectorFormSurf(0), omega(omega), J(J) { }

  template<typename Real, typename Scalar>
  Scalar vector_form_surf(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                          Geom<Real> *e, ExtData<Scalar> *ext) {
    cplx ii = cplx(0.0, 1.0);
    return ii * omega * J * int_v1<Real, Scalar>(n, wt, v); // just second component of v, since J = (0, J)
  }

  scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *u, 
               Func<double> *v, Geom<double> *e, ExtData<scalar> *ext) {
    return vector_form_surf<scalar, scalar>(n, wt, u_ext, v, e, ext);
  }

  Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, 
          Geom<Ord> *e, ExtData<Ord> *ext) {
    return vector_form_surf<Ord, Ord>(n, wt, u_ext, v, e, ext);
  }

  double omega, J;
};

class CustomWeakForm : public WeakForm
{
public:
  CustomWeakForm(double e_0, double mu_0, double mu_r, double kappa, double omega, 
                 double J, bool align_mesh) : WeakForm(1) {
    add_matrix_form(new CustomMatrixForm(e_0, mu_0, mu_r, kappa));
    add_vector_form(new CustomVectorFormSurf(omega, J));
  };
};