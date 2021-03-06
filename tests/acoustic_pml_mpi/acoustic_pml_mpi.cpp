/**
 * This program tests serial Slightly Compressible solver with a PML
 * absorbing boundary condition.
 * A Gaussian pulse is used as the time dependent BC with max velocity
 * equal to 6cm/s.
 * The PML boundary condition (1cm long) is applied to the right boundary.
 * This test takes about 400s.
 */
#include "mpi_scnsim.h"
#include "parameters.h"
#include "utilities.h"

extern template class Fluid::MPI::SCnsIM<2>;
extern template class Fluid::MPI::SCnsIM<3>;

using namespace dealii;

template <int dim>
class SigmaPMLField : public Function<dim>
{
public:
  SigmaPMLField(double sig, double l)
    : Function<dim>(), SigmaPMLMax(sig), PMLLength(l)
  {
  }
  virtual double value(const Point<dim> &p,
                       const unsigned int component = 0) const;
  virtual void value_list(const std::vector<Point<dim>> &points,
                          std::vector<double> &values,
                          const unsigned int component = 0) const;

private:
  double SigmaPMLMax;
  double PMLLength;
};

template <int dim>
double SigmaPMLField<dim>::value(const Point<dim> &p,
                                 const unsigned int component) const
{
  (void)component;
  (void)p;
  double SigmaPML = 0.0;
  double boundary = 1.4;
  // For tube acoustics
  if (p[0] > boundary - PMLLength)
    // A quadratic increasing function from boundary-PMLlength to the boundary
    SigmaPML = SigmaPMLMax * pow((p[0] + PMLLength - boundary) / PMLLength, 4);
  return SigmaPML;
}

template <int dim>
void SigmaPMLField<dim>::value_list(const std::vector<Point<dim>> &points,
                                    std::vector<double> &values,
                                    const unsigned int component) const
{
  (void)component;
  for (unsigned int i = 0; i < points.size(); ++i)
    values[i] = this->value(points[i]);
}

int main(int argc, char *argv[])
{
  using namespace dealii;

  try
    {
      Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

      std::string infile("parameters.prm");
      if (argc > 1)
        {
          infile = argv[1];
        }
      Parameters::AllParameters params(infile);

      double L = 1.4, H = 0.4;
      double PMLlength = 1.2, SigmaMax = 340000;

      auto gaussian_pulse = [dt =
                               params.time_step](const Point<2> &p,
                                                 const unsigned int component,
                                                 const double time) -> double {
        auto time_value = [](double t) {
          return 6.0 * exp(-0.5 * pow((t - 0.5e-6) / 0.15e-6, 2));
        };

        if (component == 0 && std::abs(p[0]) < 1e-10)
          return time_value(time) - time_value(time - dt);

        return 0.0;
      };

      if (params.dimension == 2)
        {
          parallel::distributed::Triangulation<2> tria(MPI_COMM_WORLD);
          dealii::GridGenerator::subdivided_hyper_rectangle(
            tria, {7, 2}, Point<2>(0, 0), Point<2>(L, H), true);
          // initialize the pml field
          auto pml = std::make_shared<SigmaPMLField<2>>(
            SigmaPMLField<2>(SigmaMax, PMLlength));
          Fluid::MPI::SCnsIM<2> flow(tria, params, pml);
          flow.add_hard_coded_boundary_condition(0, gaussian_pulse);
          flow.run();
          auto solution = flow.get_current_solution();
          // The wave is absorbed at last, so the solution should be zero.
          auto v = solution.block(0);
          double vmax = v.max();
          double verror = std::abs(vmax);
          AssertThrow(verror < 5e-2,
                      ExcMessage("Maximum velocity is incorrect!"));
        }
      else
        {
          AssertThrow(false, ExcNotImplemented());
        }
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl
                << exc.what() << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }
  return 0;
}
