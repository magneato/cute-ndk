#include <cstdio>

int main ()
{
  int array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10};

  int len = sizeof(array) / sizeof(array[0]);

  int left = 0;
  int right = len - 1;
  int pos = 0;
  int v = 7;

  while (left <= right)
  {
    pos = (left + right) / 2;
    if (array[pos] < v)
      left = pos + 1;
    else if (array[pos] > v)
      right = pos - 1;
    else 
    {
      printf("find v %d\n", pos); 
      break;
    }
  }
  left = 0;
  right = len - 1;
  while (left <= right)
  {
    pos = (left + right) / 2;

    if (array[pos] != pos)
    {
      right = pos - 1;
    }else
    {
      left = pos + 1;
    }
  }
  printf("lost %d\n", left);

  int array1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10};
  left = 0;
  len = sizeof(array1) / sizeof(array1[0]);
  right = len - 1;
  while (left <= right)
  {
    pos = (left + right) / 2;

    if (array[pos] != pos)
    {
      right = pos - 1;
    }else
    {
      left = pos + 1;
    }
  }
  printf("double %d\n", left - 1);
}
