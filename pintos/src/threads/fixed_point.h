#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#define FRACTION 14

int conv_fp (int);
int conv_int_zero (int);
int conv_int_near (int);
int add_fp_fp (int, int);
int sub_fp_fp (int, int);
int add_fp_int (int, int);
int sub_fp_int (int, int);
int mul_fp_fp (int, int);
int mul_fp_int (int, int);
int div_fp_fp (int, int);
int div_fp_int (int, int);

int conv_fp (int n)
{
  return n << FRACTION;
}

int conv_int_zero (int x)
{
  return x >> FRACTION;
}

int conv_int_near (int x)
{
  if (x >= 0)
  {
    return (x + (1 << (FRACTION - 1))) >> FRACTION;
  }
  else
  {
    return (x - (1 << (FRACTION - 1))) >> FRACTION;
  }
}

int add_fp_fp (int x, int y)
{
  return x + y;
}

int sub_fp_fp (int x, int y)
{
  return x - y;
}

int add_fp_int (int x, int n)
{
  return x + (n << FRACTION);
}

int sub_fp_int (int x, int n)
{
  return x - (n << FRACTION);
}

int mul_fp_fp (int x, int y)
{
  return (((int64_t) x) * y) >> FRACTION;
}

int mul_fp_int (int x, int n)
{
  return x * n;
}

int div_fp_fp (int x, int y)
{
  return (((int64_t) x) << FRACTION) / y;
}

int div_fp_int (int x, int n)
{
  return x / n;
}

#endif	/* threads/fixed_point.h */
