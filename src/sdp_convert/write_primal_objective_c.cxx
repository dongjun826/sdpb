#include "Dual_Constraint_Group.hxx"
#include "write_vector.hxx"
#include "../set_stream_precision.hxx"

void write_primal_objective_c(
  const boost::filesystem::path &output_dir,
  const std::vector<size_t> &indices,
  const std::vector<Dual_Constraint_Group> &dual_constraint_groups)
{
  auto block_index(indices.begin());

  for(auto &group : dual_constraint_groups)
    {
      assert(static_cast<size_t>(group.B.Height()) == group.c.size());

      boost::filesystem::ofstream output_stream(
        output_dir / ("primal_objective_c." + std::to_string(*block_index)));
      set_stream_precision(output_stream);
      write_vector(output_stream, group.c);
      ++block_index;
    }
}
