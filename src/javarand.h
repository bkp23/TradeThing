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

#ifndef JAVARAND_H
#define JAVARAND_H

#include <QtGlobal>

#define serialVersionUID = (3905348978240129619ULL)

class JavaRand
{
  public:
    JavaRand();
    JavaRand(quint64 seed);
    void setSeed(quint64 seed);
    quint32 nextInt();
    quint32 nextInt(quint32 n);
    quint64 nextLong();
    bool nextBoolean();
    float nextFloat();
    double nextDouble();

  private:
    /* Generates the next pseudorandom number.  This returns
     * a value whose bits low order bits are independent
     * chosen random bits (0 and 1 are equally likely). */
    quint32 next(int bits);

   /* True if the next nextGaussian is available.  This is used by
    * nextGaussian, which generates two gaussian numbers by one call,
    * and returns the second on the second call. */
   bool haveNextNextGaussian;
   /* The next nextGaussian, when available.  This is used by nextGaussian,
    * which generates two gaussian numbers by one call, and returns the
    * second on the second call. */
   double nextNextGaussian;
   /* The seed.  This is the number set by setSeed and which is used
    * in next. */
   quint64 seed;
};

#endif // JAVARAND_H
