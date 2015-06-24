from pysgpp import DataVector
from bin.uq.operations import evalSGFunction

import numpy as np
import pylab as plt
from bin.uq.operations.sparse_grid import dehierarchize


def plotDensity1d(U, n=1000, *args, **kws):
    bounds = U.getBounds()

    x = np.linspace(bounds[0], bounds[1], n)
    y = [U.pdf([xi]) for xi in x]

    plt.plot(x, y, *args, **kws)


def plotSGDE1d(U):
    x = np.linspace(0, 1, 11, endpoint=True)
    y = [U.pdf(xi) for xi in x]
    plt.plot(x, y)


def plotNodal1d(grid, alpha):
    gs = grid.getStorage()
    x = np.zeros(gs.size())
    nodalValues = np.zeros(gs.size())
    for i in xrange(gs.size()):
        x[i] = gs.get(i).abs(0)
        nodalValues[i] = evalSGFunction(grid, alpha, DataVector([x[i]]))

    plt.plot(x, nodalValues, " ", marker="o")


def plotSG1d(grid, alpha, n=1000, **kws):
    x = np.linspace(0, 1, n)
    y = [evalSGFunction(grid, alpha, DataVector([xi]))
         for xi in x]

    plt.plot(x, y, **kws)


def plotCDF(p, buckets):
    fig = plt.figure()
    # p, buckets = U.cdf()
    # plt.bar(buckets[:-1] + np.diff(buckets) / 2, p[1:])
    plt.plot(buckets, p, ' ', marker='o')
    plt.ylim(0, 1)
    return fig
    # x = np.linspace(buckets.min(), buckets.max(), 1000)
    # y = [self.sol_pdf.cdf(xi) for xi in x]
    # plt.plot(x, y)


def plotPDF(p, buckets):
    fig = plt.figure()
    plt.plot(buckets, p, ' ', marker='o')
    plt.ylim(0, 1)
    return fig


def plotSurplusLevelWise(data, maxLevel):
    fig = plt.figure()
    for level, surpluses in data.items():
        plt.plot([level] * len(surpluses), surpluses, ' ', marker='o')
    return fig
