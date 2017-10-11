import pysgpp
import numpy as np
import matplotlib.pyplot as plt
import math
from pysgpp.extensions.datadriven.uq.plot.plot2d import plotSG2d

pi = math.pi

class interpolation_function():
  def __init__(self, d, f):
    self.f = f
    self.d = d
    self.grid = pysgpp.Grid.createBsplineGrid(d, 3)
    self.gridStorage = self.grid.getStorage()
    try :
      self.hierarch = pysgpp.createOperationHierarchisation(self.grid)
    except :
      self.hierarch = pysgpp.createOperationMultipleHierarchisation(self.grid)
    self.opeval = pysgpp.createOperationEvalNaive(self.grid)
    self.alpha = pysgpp.DataVector(self.gridStorage.getSize())

  def create_interpolation(self, grid_lvl):
    self.gridStorage.clear()
    self.grid.getGenerator().regular(grid_lvl)
    self.alpha = pysgpp.DataVector(self.gridStorage.getSize())
    self.min_f = float('inf')
    for i in range(self.gridStorage.getSize()):
      gp = self.gridStorage.getPoint(i)
      x = [self.gridStorage.getCoordinate(gp, j) for j in range(self.d)]
      self.alpha[i] = self.f(x)
      if self.alpha[i] < self.min_f:
          self.min_f = self.alpha[i]
          self.min_x = x
    self.hierarch.doHierarchisation(self.alpha)

  def __call__(self, x):
    if (self.d == 1 and not isinstance(x, list)):
        x = [x]
    return self.opeval.eval(self.alpha, pysgpp.DataVector(x))

# ---------------------------------------------

def distrib(x):
    if isinstance(x, list):
        x = x[0]
    return (np.sin(2.5*pi*x - (pi - pi/4.)) + 1./2**0.5) / 0.527044

def parabola(x):
  res = 1.
  for i in range(len(x)):
    res *= x[i] * (1. - x[i]) * 4.;
  return res

def const_0(x):
  return 0.0

def eval_rosenblatt1d(sg_pdf, xs):
    op = pysgpp.createOperationRosenblattTransformation1D(sg_pdf.grid)
    ys = []
    for i,x in enumerate(xs):
        print("---------------{}---------------".format(i))
        ys.append(op.doTransformation1D(sg_pdf.alpha, x))
    return ys
  # ---------------------------------------------

def eval_rosenblattdd(sg_pdf, xs):
    op = pysgpp.createOperationRosenblattTransformation(sg_pdf.grid)
    X, Y = np.meshgrid(xs, xs)
    input_points = pysgpp.DataMatrix(len(xs), sg_pdf.d)
    output_points = pysgpp.DataMatrix(len(xs), sg_pdf.d)
    for i in range(len(xs)):
      for j in range(sg_pdf.d):
        input_points.set(i, j , 0.5)
    op.doTransformation(sg_pdf.alpha, input_points, output_points)
    print(output_points)

  # ---------------------------------------------


def eval_inverse_rosenblatt1d(sg_pdf, xs):
  op = pysgpp.createOperationInverseRosenblattTransformation1D(sg_pdf.grid)
  ys = []
  for i,x in enumerate(xs):
    print("---------------{}---------------".format(i))
    ys.append(op.doTransformation1D(sg_pdf.alpha, x))
  return ys

def test():
  alpha = [-7.25846502857492037464e+00, 3.43376778340746291462e+00, 3.43376778340746291462e+00, 6.70610097194139598287e+00, -2.06986834045588530273e+00, -2.06986834045588841136e+00, 6.70610097194139598287e+00]
  ys = [0, 0.151638, 0.615553, 2.39577, 3.23902, 2.39577, 0.615553, 0.230832, 0]
  print(len(alpha))
  # plt.plot(grid_points, ys)

xs = np.arange(0, 1.01, 0.01)
# xs = [0, 0.00221301, 0.25, 0.5, 0.75, 0.888787, 1.0]
l_max = 3
d = 1
interpolation = interpolation_function(d, parabola)
interpolation.create_interpolation(l_max)

# alpha = [1.97129579854422742891e+00, -6.86581347746972214807e-01, -6.86581347746972214807e-01, -9.42860151361599396758e-01, 2.91018042368639928696e-01, 2.91018042368640372786e-01, -9.42860151361599396758e-01]
# print(len(alpha))
# for i in range(0, 2**l_max - 1):
  # interpolation.alpha[i] = alpha[i]

# test()
# grid_points = np.arange(0, 1.01, 2**-l_max)
# ys = [interpolation(x) for x in xs]
# plotSG2d(interpolation.grid, interpolation.alpha)
# ys = eval_rosenblatt1d(interpolation, xs)
# print(ys)
ys = eval_inverse_rosenblatt1d(interpolation, xs)
# eval_rosenblattdd(interpolation, xs)
plt.plot(xs, ys)
# plt.scatter(grid_points, np.zeros_like(grid_points))
# plt.pcolormesh(X, Y, Z)
plt.show()
