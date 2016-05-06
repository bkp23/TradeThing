/*******************************************************************************
 * Based on Random.java -- a pseudo-random number generator
 *                         Copyright (C) 1998, 1999, 2000, 2001, 2002
 *                         Free Software Foundation, Inc.
 *
 * Copyright 2016.
 *
 * JavaRand is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/

#include "javarand.h"


 /* This class generates pseudorandom numbers.  It uses the same
  * algorithm as the original JDK-class, so that your programs behave
  * exactly the same way, if started with the same seed. */

JavaRand::JavaRand()
{
  setSeed(0); // TODO: Have this read current timestamp
}

JavaRand::JavaRand(quint64 seed)
{
  setSeed(seed);
}


void JavaRand::setSeed(quint64 seed)
{
  this->seed = (seed ^ (quint64)0x5DEECE66DULL) & (((quint64)1 << 48) - 1);
  haveNextNextGaussian = false;
}


/* Generates the next pseudorandom number.  This returns
 * a value whose bits low order bits are independent
 * chosen random bits (0 and 1 are equally likely). */
quint32 JavaRand::next(int bits)
{
  seed = (seed * (quint64)0x5DEECE66DULL + 0xB) & (((quint64)1 << 48) - 1);
  return (quint32) (seed >> (48 - bits));
}


/* Generates random bytes and places them into a user-supplied
 * byte array.  The number of random bytes produced is equal to
 * the length of the byte array.
 * NOT YET IMPLEMENTED. */
/* void JavaRand::nextBytes(byte[] bytes)
{
  int random;
  // Do a little bit unrolling of the above algorithm.
  int max = bytes.length & ~0x3;
  for (int i = 0; i < max; i += 4)
  {
    random = next(32);
    bytes[i] = (byte) random;
    bytes[i + 1] = (byte) (random >> 8);
    bytes[i + 2] = (byte) (random >> 16);
    bytes[i + 3] = (byte) (random >> 24);
  }
  if (max < bytes.length)
  {
    random = next(32);
    for (int j = max; j < bytes.length; j++)
    {
      bytes[j] = (byte) random;
      random >>= 8;
    }
  }
} */

/* Generates the next pseudorandom number.  This returns
 * an int value whose 32 bits are independent chosen random bits
 * (0 and 1 are equally likely).  */
quint32 JavaRand::nextInt()
{
  return next(32);
}

/* Generates the next pseudorandom number.  This returns
 * a value between 0(inclusive) and n(exclusive), and
 * each value has the same likelihodd (1/n). */
quint32 JavaRand::nextInt(quint32 n)
{
  if (n <= 0)
    return 0; // this should throw an error, but whatever
  if ((n & -n) == n) // i.e., n is a power of 2
    return (quint32) (((quint64)n * (quint64) next(31)) >> 31);
  quint32 bits, val;
  do
  {
    bits = next(31);
    val = bits % n;
  }
  while ((qint32)bits - (qint32)val + (qint32)(n - 1) < 0);
  return val;
}

quint64 JavaRand::nextLong()
{
  return ((quint64) next(32) << 32) + next(32);
}

bool JavaRand::nextBoolean()
{
  return next(1) != 0;
}

/* Generates the next pseudorandom float uniformly distributed
 * between 0.0f (inclusive) and 1.0f (exclusive). */
float JavaRand::nextFloat()
{
  return next(24) / (float) (1 << 24);
}

/* Generates the next pseudorandom double uniformly distributed
 * between 0.0 (inclusive) and 1.0 (exclusive). */
double JavaRand::nextDouble()
{
  return (((quint64) next(26) << 27) + next(27)) / (double) ((quint64)1 << 53);
}

/* Generates the next pseudorandom, Gaussian (normally) distributed
 * double value, with mean 0.0 and standard deviation 1.0.
 * NOT YET IMPLEMENTED */
/* double JavaRand::nextGaussian()
{
  if (haveNextNextGaussian)
  {
    haveNextNextGaussian = false;
    return nextNextGaussian;
  }
  double v1, v2, s;
  do
  {
    v1 = 2 * nextDouble() - 1; // Between -1.0 and 1.0.
    v2 = 2 * nextDouble() - 1; // Between -1.0 and 1.0.
    s = v1 * v1 + v2 * v2;
  }
  while (s >= 1);
  double norm = Math.sqrt(-2 * Math.log(s) / s);
  nextNextGaussian = v2 * norm;
  haveNextNextGaussian = true;
  return v1 * norm;
} */

