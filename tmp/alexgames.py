## This is alex's test file

from optparse import OptionParser
import sys
from tools import *
from pysgpp import *
from painlesscg import cg,sd,cg_new
from math import sqrt
import random

from array import array

try:
    import psyco
    psyco.full()
    print "Using psyco"
except:
    pass


#-------------------------------------------------------------------------------
## Outputs a deprecated warning for an option
# @param option Parameter set by the OptionParser
# @param opt Parameter set by the OptionParser
# @param value Parameter set by the OptionParser
# @param parser Parameter set by the OptionParser
def callback_deprecated(option, opt, value, parser):
    print "Warning: Option %s is deprecated." % (option)

## Reads in an ARFF file:
# The data is stored in lists. There is a value list for every dimension of the data set. e.g. 
# [[2, 3],[1, 1]] are the data points P_1(2,1) and P_2(3,1)
#
# @param filename the file's filename that should be read
# @return returns a set of a array with the data (named data), a array with the classes (named classes) and the filename named as filename
def readDataARFF(filename):
    fin = open(filename, "r")
    data = []
    classes = []
    hasclass = False

    # get the different section of ARFF-File
    for line in fin:
        sline = line.strip().lower()
        if sline.startswith("%") or len(sline) == 0:
            continue

        if sline.startswith("@data"):
            break
        
        if sline.startswith("@attribute"):
            value = sline.split()
            if value[1].startswith("class"):
                hasclass = True
            else:
                data.append([])
    
    #read in the data stored in the ARFF file
    for line in fin:
        sline = line.strip()
        if sline.startswith("%") or len(sline) == 0:
            continue

        values = sline.split(",")
        if hasclass:
            classes.append(float(values[-1]))
            values = values[:-1]
        for i in xrange(len(values)):
            data[i].append(float(values[i]))
            
    # cleaning up and return
    fin.close()
    return {"data":data, "classes":classes, "filename":filename}


def printInnerPointsOnly(grid):
    pointstring = ""
    points = None
    printpoint = True
    
    for n in xrange(grid.getStorage().size()):
        pointstring = grid.getStorage().get(n).getCoordinates()
        points = pointstring.split()
        printpoint = True
        for i in xrange(len(points)):
            if points[i] == "0":
                printpoint = False
                
            if points[i] == "1":
                printpoint = False
                
        if (printpoint == True):
            print pointstring
            

#-------------------------------------------------------------------------------
## Builds the training data vector
# 
# @param data a list of lists that contains the points a the training data set, coordinate-wise
# @return a instance of a DataVector that stores the training data
def buildTrainingVector(data):
    dim = len(data["data"])
    training = DataVector(len(data["data"][0]), dim)
    
    # i iterates over the data points, d over the dimension of one data point
    for i in xrange(len(data["data"][0])):
        for d in xrange(dim):
            training[i*dim + d] = data["data"][d][i]
    
    return training


def refinement2d():
    factory = Grid.createLinearBoundaryGrid(2)
    storage = factory.getStorage()
    
    gen = factory.createGridGenerator()
    gen.regular(1)
    
    alpha = DataVector(9)
    alpha[0] = 0.0
    alpha[1] = 0.0
    alpha[2] = 0.0
    alpha[3] = 0.0
    alpha[4] = 0.0
    alpha[5] = 0.0
    alpha[6] = 0.0
    alpha[7] = 0.0
    alpha[8] = 1.0
    func = SurplusRefinementFunctor(alpha)
    
    #for n in xrange(factory.getStorage().size()):
    #    print factory.getStorage().get(n).getCoordinates()
    
    gen.refine(func)

    for n in xrange(factory.getStorage().size()):
        print factory.getStorage().get(n).getCoordinates()    


def refinement3d():
    factory = Grid.createLinearBoundaryGrid(3)
    storage = factory.getStorage()
    
    gen = factory.createGridGenerator()
    gen.regular(1)
 
    #for n in xrange(factory.getStorage().size()):
    #    print factory.getStorage().get(n).getCoordinates() 
    
    alpha = DataVector(27)
    
    for i in xrange(len(alpha)):
        alpha[i] = 0.0

    alpha[26] = 1.0
    func = SurplusRefinementFunctor(alpha)
            
    gen.refine(func)

    for n in xrange(factory.getStorage().size()):
        print factory.getStorage().get(n).getCoordinates()


def generateLaplaceMatrix(factory, level, verbose=False):
    from pysgpp import DataVector
    storage = factory.getStorage()
    
    gen = factory.createGridGenerator()
    gen.regular(level)
    
    laplace = factory.createOperationLaplace()
    
    # create vector
    alpha = DataVector(storage.size())
    erg = DataVector(storage.size())

    # create stiffness matrix
    m = DataVector(storage.size(), storage.size())
    m.setAll(0)
    for i in xrange(storage.size()):
        # apply unit vectors
        alpha.setAll(0)
        alpha[i] = 1
        laplace.mult(alpha, erg)
        if verbose:
            print erg, erg.sum()
        m.setColumn(i, erg)

    return m
        
def printDiagonal(m1):
    n = m1.getSize()

    # check diagonal
    values = []
    for i in range(n):
        values.append(m1[i*n + i])
    #values.sort()
    for i in range(n):
        print values[i]
        
def roundVector(v):
    for i in xrange(len(v)):
        v[i] = round(v[i], 10)
    
    return v
  
def printMatrix(m):
    n = m.getSize()

    for i in range(n):
        values = []
        for j in range(n):
            values.append(m[i*n + j])
        
        print roundVector(values)
        
    return
    
    
                       
# Alex is playing with python and sgpp
#
# This test routine creates a grid and evaluates a function
# on this grid
def run_test():
    factory = Grid.createLinearBoundaryUScaledGrid(2)
    m = generateLaplaceMatrix(factory, 1)
    
    #print str(m) 
    #printDiagonal(m)
    
    print "C down: Gitter mit Rand:"
    printMatrix(m)
    print "\n\n"

    factorytwo = Grid.createLinearGrid(2)
    mtwo = generateLaplaceMatrix(factorytwo, 1)
    
    #print str(m) 
    #printDiagonal(m)
    
    print "C down: Gitter ohne Rand:"
    printMatrix(mtwo)
    
    return
      
#===============================================================================
# Main
#===============================================================================

# check so that file can also be imported in other files
if __name__=='__main__':
    #start the test programm
    run_test()
