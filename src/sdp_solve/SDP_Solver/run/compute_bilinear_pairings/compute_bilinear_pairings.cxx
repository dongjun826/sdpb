#include "../../../Block_Diagonal_Matrix.hxx"
#include "../../../../Timers.hxx"

void compute_bilinear_pairings_X_inv(
  const Block_Diagonal_Matrix &X_cholesky,
  const std::vector<El::DistMatrix<El::BigFloat>> &bases_blocks,
  Block_Diagonal_Matrix &bilinear_pairings_X_inv);

void compute_bilinear_pairings_Y(
  const Block_Diagonal_Matrix &Y,
  const std::vector<El::DistMatrix<El::BigFloat>> &bases_blocks,
  Block_Diagonal_Matrix &bilinear_pairings_Y);

void compute_bilinear_pairings(
  const Block_Diagonal_Matrix &X_cholesky, const Block_Diagonal_Matrix &Y,
  std::vector<El::DistMatrix<El::BigFloat>> &bases_blocks,
  Block_Diagonal_Matrix &bilinear_pairings_X_inv,
  Block_Diagonal_Matrix &bilinear_pairings_Y, Timers &timers)
{
  auto &congruence_timer(timers.add_and_start("run.bilinear_pairings"));
  compute_bilinear_pairings_X_inv(X_cholesky, bases_blocks,
                                  bilinear_pairings_X_inv);

  compute_bilinear_pairings_Y(Y, bases_blocks, bilinear_pairings_Y);
  congruence_timer.stop();
}
