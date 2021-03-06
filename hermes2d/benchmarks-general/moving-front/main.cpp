#define HERMES_REPORT_ALL
#define HERMES_REPORT_FILE "application.log"
#include "definitions.h"

using namespace RefinementSelectors;

//  This benchmark has an exact solution that exhibits a moving front or 
//  arbitrary steepness "s". 
//
//  PDE: time-dependent heat transfer equation, du/dt = Laplace u + f.
//
//  Domain: square (0, 10) \times (-10, 10).
//
//  BC: Zero Dirichlet.
//
//  IC: Zero.
//
//  The following parameters can be changed:

const int INIT_GLOB_REF_NUM = 4;                   // Number of initial uniform mesh refinements.
const int INIT_BDY_REF_NUM = 0;                    // Number of initial refinements towards boundary.
const int P_INIT = 2;                              // Initial polynomial degree.
double time_step = 0.01;                           // Time step.
const double T_FINAL = 10.0;                        // Time interval length.
const double NEWTON_TOL = 1e-5;                    // Stopping criterion for the Newton's method.
const int NEWTON_MAX_ITER = 100;                   // Maximum allowed number of Newton iterations.
const double TIME_TOL_UPPER = 5.0;                 // If rel. temporal error is greater than this threshold, decrease time 
                                                   // step size and repeat time step.
const double TIME_TOL_LOWER = 0.5;                 // If rel. temporal error is less than this threshold, increase time step
                                                   // but do not repeat time step (this might need further research).
const double TIME_STEP_INC_RATIO = 1.1;            // Time step increase ratio (applied when rel. temporal error is too small).
const double TIME_STEP_DEC_RATIO = 0.8;            // Time step decrease ratio (applied when rel. temporal error is too large).
MatrixSolverType matrix_solver = SOLVER_UMFPACK;   // Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
                                                   // SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.

// Choose one of the following time-integration methods, or define your own Butcher's table. The last number 
// in the name of each method is its order. The one before last, if present, is the number of stages.
// Explicit methods:
//   Explicit_RK_1, Explicit_RK_2, Explicit_RK_3, Explicit_RK_4.
// Implicit methods:
//   Implicit_RK_1, Implicit_Crank_Nicolson_2_2, Implicit_SIRK_2_2, Implicit_ESIRK_2_2, Implicit_SDIRK_2_2, 
//   Implicit_Lobatto_IIIA_2_2, Implicit_Lobatto_IIIB_2_2, Implicit_Lobatto_IIIC_2_2, Implicit_Lobatto_IIIA_3_4, 
//   Implicit_Lobatto_IIIB_3_4, Implicit_Lobatto_IIIC_3_4, Implicit_Radau_IIA_3_5, Implicit_SDIRK_5_4.
// Embedded explicit methods:
//   Explicit_HEUN_EULER_2_12_embedded, Explicit_BOGACKI_SHAMPINE_4_23_embedded, Explicit_FEHLBERG_6_45_embedded,
//   Explicit_CASH_KARP_6_45_embedded, Explicit_DORMAND_PRINCE_7_45_embedded.
// Embedded implicit methods:
//   Implicit_SDIRK_CASH_3_23_embedded, Implicit_ESDIRK_TRBDF2_3_23_embedded, Implicit_ESDIRK_TRX2_3_23_embedded, 
//   Implicit_SDIRK_BILLINGTON_3_23_embedded, Implicit_SDIRK_CASH_5_24_embedded, Implicit_SDIRK_CASH_5_34_embedded, 
//   Implicit_DIRK_ISMAIL_7_45_embedded. 
ButcherTableType butcher_table_type = Implicit_SDIRK_CASH_3_23_embedded;

// Problem parameters.
double x_0 = 0.0;
double x_1 = 10.0;
double y_0 = -5.0;
double y_1 = 5.0;
double s = 2.0;
double c = 10000.0;

// Current time.
double current_time = 0.0;

int main(int argc, char* argv[])
{
  // Instantiate a class with global functions.
  Hermes2D hermes2d;

  // Choose a Butcher's table or define your own.
  ButcherTable bt(butcher_table_type);
  if (bt.is_explicit()) info("Using a %d-stage explicit R-K method.", bt.get_size());
  if (bt.is_diagonally_implicit()) info("Using a %d-stage diagonally implicit R-K method.", bt.get_size());
  if (bt.is_fully_implicit()) info("Using a %d-stage fully implicit R-K method.", bt.get_size());

  // Load the mesh.
  Mesh mesh;
  H2DReader mloader;
  mloader.load("domain.mesh", &mesh);

  // Initial mesh refinements.
  for(int i = 0; i < INIT_GLOB_REF_NUM; i++) mesh.refine_all_elements();
  mesh.refine_towards_boundary("Bdy", INIT_BDY_REF_NUM);

  // Exact solution.
  CustomExactSolution exact_sln(&mesh, x_0, x_1, y_0, y_1, &current_time, s, c);

  // Initialize boundary conditions.
  DefaultEssentialBCConst bc_essential("Bdy", 0);
  EssentialBCs bcs(&bc_essential);

  // Create an H1 space with default shapeset.
  H1Space space(&mesh, &bcs, P_INIT);
  int ndof = space.get_num_dofs();

  // Convert initial condition into a Solution.
  Solution sln_time_prev(&mesh, 0);
  Solution sln_time_new(&mesh);
  Solution time_error_fn(&mesh, 0.0);

  // Initialize the weak formulation
  CustomFunction f(x_0, x_1, y_0, y_1, s, c);
  CustomWeakFormPoisson wf(HERMES_ANY, new HermesFunction(-1.0), &f);

  // Initialize the discrete problem.
  DiscreteProblem dp(&wf, &space);

  // Initialize views.
  ScalarView sview_high("Solution (higher-order)", new WinGeom(0, 0, 500, 400));
  ScalarView est_view("Estimated temporal error", new WinGeom(505, 0, 500, 400));
  est_view.fix_scale_width(60);
  ScalarView exact_view("Exact temporal error", new WinGeom(1010, 0, 500, 400));
  exact_view.fix_scale_width(60);

  RungeKutta runge_kutta(&dp, &bt, matrix_solver);

  // Graph for time step history.
  SimpleGraph time_step_graph, err_est_graph, err_exact_graph;
  info("Time step history will be saved to file time_step_history.dat.");

  // Time stepping loop:
  int ts = 1;
  do 
  {
    // Perform one Runge-Kutta time step according to the selected Butcher's table.
    info("Runge-Kutta time step (t = %g, tau = %g, stages: %d).", 
         current_time, time_step, bt.get_size());
    bool verbose = true;
    bool jacobian_changed = true;
    if (!runge_kutta.rk_time_step(current_time, time_step, &sln_time_prev, 
                                  &sln_time_new, &time_error_fn, jacobian_changed, verbose, 
                                  NEWTON_TOL, NEWTON_MAX_ITER)) {
      error("Runge-Kutta time step failed, try to decrease time step size.");
    }

    // Plot error estimate and exact error.
    char title[100];
    sprintf(title, "Estimated temporal error, t = %g", current_time);
    est_view.set_title(title);
    AbsFilter abs_tef(&time_error_fn);
    est_view.show(&abs_tef, HERMES_EPS_VERYHIGH);
    DiffFilter exact_efn(Hermes::vector<MeshFunction*>(&sln_time_new, &exact_sln),
                         Hermes::vector<int>(H2D_FN_VAL, H2D_FN_VAL));
    AbsFilter exact_efn_abs(&exact_efn);
    exact_view.show(&exact_efn_abs, HERMES_EPS_VERYHIGH);

    // Calculate relative temporal error estimate and exact error.
    double rel_err_est = hermes2d.calc_norm(&time_error_fn, HERMES_H1_NORM) / 
                         hermes2d.calc_norm(&sln_time_new, HERMES_H1_NORM) * 100;
    double rel_err_exact = hermes2d.calc_abs_error(&exact_sln, &sln_time_new, HERMES_H1_NORM) / 
                           hermes2d.calc_norm(&exact_sln, HERMES_H1_NORM) * 100;
    info("rel_err_est = %g%%", rel_err_est);
    info("rel_err_exact = %g%%", rel_err_exact);

    // Add entries to error graphs.
    err_est_graph.add_values(current_time, rel_err_est);
    err_est_graph.save("err_est_history.dat");
    err_exact_graph.add_values(current_time, rel_err_exact);
    err_exact_graph.save("err_exact_history.dat");
    
    // Decide whether the time step can be accepted. If not, then the 
    // time step size is reduced and the entire time step repeated. 
    // If yes, then another check is run, and if the relative error 
    // is very low, time step is increased.
    if (rel_err_est > TIME_TOL_UPPER) {
      info("rel_err_est above upper limit %g%% -> decreasing time step from %g to %g and repeating time step.", 
           TIME_TOL_UPPER, time_step, time_step * TIME_STEP_DEC_RATIO);
      time_step *= TIME_STEP_DEC_RATIO;
      continue;
    }
    if (rel_err_est < TIME_TOL_LOWER) {
      info("rel_err_est = below lower limit %g%% -> increasing time step from %g to %g", 
           TIME_TOL_UPPER, time_step, time_step * TIME_STEP_INC_RATIO);
      time_step *= TIME_STEP_INC_RATIO;
    }
   
    // Add entry to time graphs.
    time_step_graph.add_values(current_time, time_step);
    time_step_graph.save("time_step_history.dat");

    // Show the new time level solution.
    sprintf(title, "Solution (higher-order), t = %g", current_time);
    sview_high.set_title(title);
    sview_high.show(&sln_time_new, HERMES_EPS_HIGH);

    // Copy solution for next time step.
    sln_time_prev.copy(&sln_time_new);

    // Update time.
    current_time += time_step;

    // Increase counter of time steps.
    ts++;

    //View::wait(HERMES_WAIT_KEYPRESS);

  } 
  while (current_time < T_FINAL);

  // Wait for all views to be closed.
  View::wait();
  return 0;
}
