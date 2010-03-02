#include <iostream>

const int M = 5;
const int N = 3;
int main ()
{
  int a[M][N] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  int c[N][M] = {0};

  int t = 0;

  for (; t < M*N; ++t)
    c[t% N][M - 1 - t/N] = a[t/N][t%N];
  for (t = 0; t < M*N; t++)
  {
    if (t % N == 0)
      std::cout << std::endl;
    std::cout << a[t/N][t%N] << ' ';
  }
  std::cout << std::endl;

  for (t = 0; t < M * N; ++t)
  {
    if (t % M == 0)
      std::cout << std::endl;
    std::cout << a[t/M][t%M] << ' ';
  }
  std::cout << std::endl;
}
