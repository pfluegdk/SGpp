from argparse import ArgumentParser
from mpl_toolkits.mplot3d import Axes3D
import os
import time
from matplotlib import cm
import active_subspaces as ac
import asFunctions
import cPickle as pickle
import matplotlib.pyplot as plt
import numpy as np
import pysgpp


# SG++ AS functionalities return eigenvalues as increasingly sorted DataVector.
# reverse the order and cast to numpy.ndarray
def reverseDataVectorToNdArray(v):
    n = np.ndarray(v.getSize())
    for i in range(v.getSize()):
        n[-i - 1] = v[i]
    return n


# SG++ AS functionalities return eigenvectors as increasingly sorted (by eigenvalues)
# DataMatrix. reverse the order and cast to numpy.ndarray
def reverseDataMatrixToNdArray(m):
    n = np.ndarray(shape=(m.getNrows(), m.getNcols()))
    for i in range(m.getNcols()):
        for j in range(m.getNrows()):
            n[i, -j - 1] = m.get(i, j)
    return n


# wraps the objective function for SGpp    
class objFuncSGpp(pysgpp.OptScalarFunction):

    def __init__(self, objFunc):
        self.numDim = objFunc.getDim()
        self.objFunc = objFunc
        super(objFuncSGpp, self).__init__(self.numDim)

    def eval(self, v):
        x = np.ndarray(shape=(1, self.numDim))
        for i in range(self.numDim):
            x[0, i] = v[i]
        return  self.objFunc.eval(x)[0][0]

    
# uniformly distributed points in numDim dimensions     
def uniformX(numSamples, numDim):
    x = np.ndarray(shape=(numSamples, numDim))
    for d in range(numDim):
        r = np.random.uniform(-1, 1, (numSamples, 1))
        x[:, d] = r[:, 0]
    return x


# points distributed according to the borehole example
def boreholeX(numSamples):
    rw = np.random.normal(.1, .0161812, (numSamples, 1))
    r = np.exp(np.random.normal(7.71, 1.0056, (numSamples, 1)))
    Tu = np.random.uniform(63070, 115600, (numSamples, 1))
    Hu = np.random.uniform(990, 1110, (numSamples, 1))
    Tl = np.random.uniform(63.1, 116, (numSamples, 1))
    Hl = np.random.uniform(700, 820, (numSamples, 1))
    L = np.random.uniform(1120, 1680, (numSamples, 1))
    Kw = np.random.uniform(9855, 12045, (numSamples, 1))
    x_t = np.hstack((rw, r, Tu, Hu, Tl, Hl, L, Kw))
    xl = np.array([63070, 990, 63.1, 700, 1120, 9855])
    xu = np.array([115600, 1110, 116, 820, 1680, 12045])
    # XX = normalized input matrix
    XX = ac.utils.misc.BoundedNormalizer(xl, xu).normalize(x_t[:, 2:])
    # normalize non-uniform inputs
    rw_norm = ((rw - .1) / .0161812).reshape(numSamples, 1)
    r_norm = np.log(r); r_norm = ((r_norm - 7.71) / 1.0056).reshape(numSamples, 1)
    XX = np.hstack((rw_norm, r_norm, XX))
    return XX


#---------------------usual SGpp interpolant----------------------------
# objFunc        objective function
# gridType       type of basis functions
# degree         degree of basis functions
# numResponse    number of points (adaptive) or level (regular) of the response surface
# responseType   method for creation of the response surface. Adaptive or regular
# numerrorPoints number of MC points used to calculate the l2 interpolation error
#-----------------------------------------------------------------------
def SGpp(objFunc, gridType, degree, numResponse, responseType='adaptive', numErrorPoints=10000):
    pysgpp.OptPrinter.getInstance().setVerbosity(-1)
    numDim = objFunc.getDim()
    f = objFuncSGpp(objFunc)
    
    sparseResponseSurf = pysgpp.SparseGridResponseSurfaceNakBspline(f, \
                                                                    pysgpp.Grid.stringToGridType(gridType), degree)
    if responseType == 'adaptive':
        initialLevel = 1
        sparseResponseSurf.createSurplusAdaptiveResponseSurface(numResponse, initialLevel)
    elif responseType == 'regular':
        sparseResponseSurf.createRegularResponseSurface(numResponse)
     
    print(numResponse)   
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    l2Error = sparseResponseSurf.l2Error(f, numErrorPoints)
    print("sparse interpolation error {}".format(l2Error))
    integral = sparseResponseSurf.getIntegral() * vol
    integralError = abs(integral - objFunc.getIntegral())
    print("sparse integral: {}".format(integral))
    print("sparse integral error {}\n".format(integralError))
    
    return l2Error, integral, integralError


#---------------------SGpp with active subspaces----------------------------
# objFunc        the objective Function
# gridType       type of the basis functions
# degree         degree of the basis functions
# numASM         number of points (adaptive) or level (regular) for the creation of the ASM
# numRepsonse    number of points (adaptive) or level (regular,detection) of the response surface
# asmType        method for creation of the ASM (adaptive or regular)
# responseType   method for creation of the response surface. Adaptive, regular or from the detection points
# integralType   Monte Carlo integral ('MC') or semi continuous integral ('Cont')
# numErrorPoints number of MC points used to calculate the l2 interpolation error
# savePath       path to save the interpolation grid and coefficients to or None
#--------------------------------------------------------------------------
def SGppAS(objFunc, gridType, degree, numASM, numResponse, asmType='adaptive',
                responseType='adaptive', integralType='Hist', numErrorPoints=10000,
                numHistogramMCPoints=1000000, savePath=None, approxLevel=8,
                approxDegree=3, numShadow1DPoints=0):
    print("{}".format(numASM))
    pysgpp.OptPrinter.getInstance().setVerbosity(-1)
    numDim = objFunc.getDim()
    f = objFuncSGpp(objFunc)
    lb, ub = objFunc.getDomain()
    numRefine = 3
    initialLevel = 1
    
    if asmType in ["adaptive", "regular"]:
        ASM = pysgpp.ASMatrixBsplineAnalytic(f, pysgpp.Grid.stringToGridType(gridType), degree)
        if asmType == 'adaptive':
            ASM.buildAdaptiveInterpolant(numASM, initialLevel, numRefine)
        elif asmType == 'regualar':
            ASM.buildRegularInterpolant(numASM)
    elif asmType in ["data"]:
        numPoints = 10000
        randomPoints = np.random.rand(numDim, numPoints)
        evaluationPoints = pysgpp.DataMatrix(numDim, numPoints)
        for i in range(numDim):
            for j in range(numPoints):
                evaluationPoints.set(i, j, randomPoints[i, j])
        functionValues = pysgpp.DataVector(numPoints)
        for i in range(numPoints):
            functionValues[i] = f.eval(randomPoints[:, i])
        ASM = pysgpp.ASMatrixBsplineData(evaluationPoints, functionValues, pysgpp.Grid.stringToGridType(gridType), degree)
        # asmLevel = 5
        # ASM.buildRegularInterpolant(asmLevel)
        ASM.buildAdaptiveInterpolant(numASM)
    
    if savePath is not None:
        if not os.path.exists(savePath):
            os.makedirs(savePath)
        ASM.toFile(savePath)
 
# plot function and asm interpolant for 1D objective function       
#     X = np.linspace(0, 1, 100)
#     F = [f.eval([x]) for x in X]
#     I = [ASM.evalInterpolant(pysgpp.DataVector(1, x)) for x in X]
#     EP = np.zeros(numPoints)
#     FV = np.zeros(numPoints)
#     for j in range(numPoints):
#         EP[j] = evaluationPoints.get(0, j)
#         FV[j] = functionValues[j]
#     plt.plot(X, F, 'b')
#     plt.plot(X, I, 'r')
#     plt.plot(EP, FV, 'yo')
#     plt.show()
    
    ASM.createMatrixGauss()
    ASM.evDecompositionForSymmetricMatrices()
    eivalSGpp = ASM.getEigenvaluesDataVector()
    eivecSGpp = ASM.getEigenvectorsDataMatrix()
    eival = reverseDataVectorToNdArray(eivalSGpp)
    eivec = reverseDataMatrixToNdArray(eivecSGpp)
    
#     print(eival)
#     print(eivec)
#     print("AS:")
#     print(eivec[:, 0])
    
    n = 1  # active subspace identifier
    responseDegree = degree  # test if different degrees for ASM and resposne surface are useful!
    responseGridType = pysgpp.GridType_NakBsplineBoundary  # NakBsplineExtended  # test if other gridTypes are useful!
    responseSurf = ASM.getResponseSurfaceInstance(n, responseGridType, responseDegree)
    if responseType == 'adaptive':
        responseSurf.createAdaptiveReducedSurfaceWithPseudoInverse(numResponse, f, initialLevel, numRefine)
    elif responseType == 'regular':
        responseSurf.createRegularReducedSurfaceWithPseudoInverse(numResponse, f)
    elif responseType == 'data':
        asmPoints = ASM.getEvaluationPoints()
        asmValues = ASM.getFunctionValues()
        if savePath is not None:
            if not os.path.exists(savePath):
                os.makedirs(savePath)
            asmPoints.toFile(os.path.join(savePath, 'asmPoints' + str(numResponse) + '.dat'))
            asmValues.toFile(os.path.join(savePath, 'asmValues' + str(numResponse) + '.dat'))
        responseLevel = 5
        responseSurf.createRegularReducedSurfaceFromData(asmPoints, asmValues, responseLevel)
#         responseSurf.createRegularReducedSurfaceFromData_DataDriven(asmPoints, asmValues, responseLevel)
#         responseSurf.createAdaptiveReducedSurfaceFromData(numResponse, asmPoints, asmValues)

    bounds = responseSurf.getBounds() 
    print("leftBound: {} rightBound: {}".format(bounds[0], bounds[1]))
    
    l2Error = responseSurf.l2Error(f, numErrorPoints)
    print("interpol error: {}".format(l2Error))
    
    shadow1DEvaluations = []
    if numShadow1DPoints > 0:
        X1unit = np.linspace(0, 1, numShadow1DPoints)
        shadow1DEvaluations = [responseSurf.eval1D(x)  for x in X1unit]

    vol = np.prod(ub - lb)
    integral = float('NaN')
    if integralType == 'MC':
        integral = responseSurf.getMCIntegral(100, numHistogramMCPoints, 'Halton') * vol
    elif integralType == 'Hist':
        integral = responseSurf.getHistogramBasedIntegral(11, numHistogramMCPoints, 'Halton') * vol
    elif integralType == 'Spline':
        quadOrder = 7
        integral = responseSurf.getSplineBasedIntegral(quadOrder) * vol
    elif integralType == 'appSpline':
        integral = responseSurf.getApproximateSplineBasedIntegral(approxLevel, approxDegree) * vol
    print("integral: {}\n".format(integral)),
    integralError = abs(integral - objFunc.getIntegral())
    print("integral error: {}".format(integralError))
    
# plot interpolant of 2D function
#     X, Y = np.meshgrid(np.linspace(0, 1, 100), np.linspace(0, 1, 100))
#     I = np.zeros(np.shape(X))
#     F = np.ndarray(np.shape(X))
#     for i in range(len(X)):
#         for j in range(len(X[0])):
#             I[i, j] = responseSurf.eval(pysgpp.DataVector([X[i, j], Y[i, j]]))
#             F[i, j] = f.eval([X[i, j], Y[i, j]])
#     fig = plt.figure(); ax = fig.gca(projection='3d')
#     ax.plot_surface(X, Y, I, cmap=cm.viridis, linewidth=0, antialiased=False)
#     ax.plot_wireframe(X, Y, F, rstride=10, cstride=10,color='r')
#     plt.show()

    return eival, eivec, l2Error, integral, integralError, shadow1DEvaluations, [bounds[0], bounds[1]]


#---------------------Constantines AS framework----------------------------
# X        the evaluation points
# f        the objective function evaluated at X
# df       the objective functions gradient evaluated at X
# sstype   gradient based ('AS'), linear fit ('OLS') or quadratic fit ('QPHD')
# nboot    number of bootstrappings
#-------------------------------------------------------------------------- 
def ConstantineAS(X=None, f=None, df=None, responseDegree=2, sstype='AS', nboot=0, numShadow1DPoints=0):
    ss = ac.subspaces.Subspaces()
     #----------- linear fit ----------
    if sstype == 'OLS':
        ss.compute(X=X, f=f, nboot=nboot, sstype='OLS')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
    #----------- quadratic fit -----------
    elif sstype == 'QPHD':
        ss.compute(X=X, f=f, nboot=nboot, sstype='QPHD')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
    # ---------- exact gradient ----------
    elif sstype == 'AS':
        ss.compute(df=df, nboot=nboot, sstype='AS')
        eival = ss.eigenvals
        eivec = ss.eigenvecs
    
    # quadratic polynomial approximation of maximum degree responseDegree
    print("response degree: {}".format(responseDegree))
    RS = ac.utils.response_surfaces.PolynomialApproximation(responseDegree)
    # RS = ac.utils.response_surfaces.RadialBasisApproximation(responseDegree)
    # Train the surface with active variable values (y = XX.dot(ss.W1)) and function values (f)
    y = X.dot(ss.W1)
    RS.train(y, f)
    
    # calculate l2 error in W1T * [-1,1]. 
    numErrorPoints = 10000
    errorPoints = np.random.random((numErrorPoints, objFunc.getDim())) * 2 - 1
    errorEval = RS.predict(errorPoints.dot(ss.W1))[0]
    errorFunctionValues = objFunc.eval(errorPoints)
    l2Error = np.linalg.norm((errorEval - errorFunctionValues) / numErrorPoints)
    print"interpol error {}".format(l2Error),
    
    # integral
    lb, ub = objFunc.getDomain()
    vol = np.prod(ub - lb)
    avdom = ac.domains.BoundedActiveVariableDomain(ss)
    avmap = ac.domains.BoundedActiveVariableMap(avdom)  
    integral = ac.integrals.av_integrate(lambda x: RS.predict(x)[0], avmap, 1000) * vol  # / (2 ** objFunc.getDim())
#     print 'Constantine Integral: {:.2f}'.format(int_I)
    integralError = abs(integral - objFunc.getIntegral())
    print(" integral error {}".format(integralError))
    
    bounds = avdom.vertY[ :, 0]
    shadow1DEvaluations = []
    if numShadow1DPoints > 0:
        X1 = np.ndarray((numShadow1DPoints, 1))
        X1[:, 0] = np.linspace(bounds[0], bounds[1], numShadow1DPoints)
        shadow1DEvaluations = RS.predict(X1)[0]
    
    # plot interpolant of 2D function
#     X, Y = np.meshgrid(np.linspace(-1, 1, 50), np.linspace(-1, 1, 50))
#     Xunit, Yunit = np.meshgrid(np.linspace(0, 1, 50), np.linspace(0, 1, 50))
#     I = np.zeros(np.shape(X))
#     F = np.ndarray(np.shape(X))
#     wrapf = objFuncSGpp(objFunc)
#     for i in range(len(X)):
#         for j in range(len(X[0])):
#             p = np.ndarray((1, 2))
#             p[0, 0] = X[i, j]
#             p[0, 1] = Y[i, j]
#             I[i, j] = RS.predict(p.dot(ss.W1))[0]
#             F[i, j] = wrapf.eval([Xunit[i, j], Yunit[i, j]])
#     fig = plt.figure(); ax = fig.gca(projection='3d')
#     ax.plot_surface(Xunit, Yunit, I, cmap=cm.viridis, linewidth=0, antialiased=False)
#     ax.plot_wireframe(Xunit, Yunit, F, rstride=10, cstride=10, color='r')
#     # ax.scatter((errorPoints[:, 0] + 1) / 2.0, (errorPoints[:, 1] + 1) / 2.0, errorEval, c='b')
#     plt.show()
        
    return eival, eivec, l2Error, integral, integralError, shadow1DEvaluations, bounds


#------------------------------------ main ---------------------------------------
if __name__ == "__main__":
    # parse the input arguments
    parser = ArgumentParser(description='Get a program and run it with input', version='%(prog)s 1.0')
    parser.add_argument('--model', default='sin5Dexp0.1', type=str, help="define which test case should be executed")
    parser.add_argument('--method', default='QPHD', type=str, help="asSGpp, SGpp or one of the three Constantine (AS,OLS,QPHD)")
    parser.add_argument('--minPoints', default=10, type=int, help="minimum number of points used")
    parser.add_argument('--maxPoints', default=100, type=int, help="maximum number of points used")
    parser.add_argument('--numSteps', default=5, type=int, help="number of steps in the [minPoints maxPoints] range")
    parser.add_argument('--saveFlag', default=1, type=bool, help="save results")
    parser.add_argument("--numShadow1DPoints", default=100, type=int, help="number of evaluations of the underlying 1D interpolant which can later be used for shadow plots")
    # only relevant for asSGpp and SGpp
    parser.add_argument('--gridType', default='nakbsplinemodified', type=str, help="SGpp grid type")
    parser.add_argument('--degree', default=3, type=int, help="B-spline degree / degree of Constantines resposne surface")
    parser.add_argument('--responseType', default='adaptive', type=str, help="method for response surface creation (regular,adaptive (and detection for asSGpp) ")
    # only relevant for asSGpp
    parser.add_argument('--asmType', default='adaptive', type=str, help="method for ASM creation (regular adaptive)")
    parser.add_argument('--integralType', default='Spline', type=str, help="method for integral calculation (MC, Cont)")
    parser.add_argument('--appSplineLevel', default=5, type=int, help="level used for integralType appSpline")
    parser.add_argument('--appSplineDegree', default=3, type=int, help="degree used for integralType appSpline")
    args = parser.parse_args()

    objFunc = asFunctions.getFunction(args.model)
    numDim = objFunc.getDim()
    sampleRange = np.unique(np.logspace(np.log10(args.minPoints), np.log10(args.maxPoints), num=args.numSteps))
    sampleRange = [int(s) for s in sampleRange]
    
    eival = np.ndarray(shape=(numDim , len(sampleRange)))
    eivec = np.ndarray(shape=(numDim, numDim, len(sampleRange)))
    durations = [0] * len(sampleRange); l2Errors = [0] * len(sampleRange)
    integrals = [0] * len(sampleRange); integralErrors = [0] * len(sampleRange)
    shadow1DEvaluationsArray = np.ndarray(shape=(args.numShadow1DPoints, len(sampleRange)))
    boundsArray = np.ndarray(shape=(2, len(sampleRange)))
    
    resultsPath = "/home/rehmemk/git/SGpp/activeSubSpaces/results"
    resultsPath = os.path.join(resultsPath, objFunc.getName())
    if args.method in ['OLS', 'QPHD', 'AS']:
        folder = args.method + '_' + str(args.degree) + '_' + str(args.maxPoints)
    elif args.method == 'asSGpp': 
        folder = args.method + '_' + args.gridType + '_' + str(args.degree) + '_' + str(args.maxPoints) + '_' + args.responseType + '_' + args.asmType + '_' + args.integralType
    elif args.method == 'SGpp': 
        folder = args.method + '_' + args.gridType + '_' + str(args.degree) + '_' + str(args.maxPoints) + '_' + args.responseType
    path = os.path.join(resultsPath, folder)     
    
    numHistogramMCPoints = 100000
    
    # .... ..... .... Constantines Code .... .... ....
    if args.method in ['AS', 'OLS', 'QPHD']:
        nboot = 100
        for i, numSamples in enumerate(sampleRange):
            start = time.time()
            if args.model == 'borehole':
                x = boreholeX(numSamples)
            else:
                x = uniformX(numSamples, numDim)
            f = objFunc.eval(x, -1, 1)
            if args.method == 'AS':
                df = objFunc.eval_grad(x, -1, 1)
                e, v, l2Error, integral, integralError, shadow1DEvaluations, bounds = ConstantineAS(X=x, f=f, df=df, responseDegree=args.degree,
                                                                        sstype=args.method, nboot=nboot,
                                                                        numShadow1DPoints=args.numShadow1DPoints)
            else:
                e, v, l2Error, integral, integralError, shadow1DEvaluations, bounds = ConstantineAS(X=x, f=f, responseDegree=args.degree,
                                                                       sstype=args.method, nboot=nboot,
                                                                       numShadow1DPoints=args.numShadow1DPoints)
            durations[i] = time.time() - start; l2Errors[i] = l2Error;
            integrals[i] = integral; integralErrors[i] = integralError
            shadow1DEvaluationsArray[:, i] = shadow1DEvaluations[:, 0]
            boundsArray[:, i] = bounds
            eival[:, i] = e[:, 0]; eivec[:, :, i] = v
            
    # .... .... .... active subspace SG++ .... .... .... 
    elif args.method == 'asSGpp':
        computeIntegral = False
        initialLevel = 1
        numRefine = 3
        for i, numSamples in enumerate(sampleRange):
            start = time.time()
            numResponse = numSamples
            numASM = numSamples
            e, v, l2Error, integral, integralError, shadow1DEvaluations, bounds = SGppAS(objFunc,
                                                             args.gridType, args.degree, numASM, numResponse,
                                                             args.asmType, args.responseType, args.integralType,
                                                             numErrorPoints=10000, savePath=path,
                                                             numHistogramMCPoints=numHistogramMCPoints,
                                                             approxLevel=args.appSplineLevel, approxDegree=args.appSplineDegree,
                                                             numShadow1DPoints=args.numShadow1DPoints)
            durations[i] = time.time() - start; l2Errors[i] = l2Error;
            integrals[i] = integral; integralErrors[i] = integralError
            shadow1DEvaluationsArray[:, i] = shadow1DEvaluations
            boundsArray[:, i] = bounds
            eival[:, i] = e[:]
            eivec[:, :, i] = v
            
    # .... .... .... .... SG++ .... .... .... ....
    elif args.method == 'SGpp':
        initialLevel = 1
        numRefine = 3
        for i, numSamples in enumerate(sampleRange):
            start = time.time()
            l2Error, integral, integralError = SGpp(objFunc, args.gridType, args.degree, numSamples,
                                                     args.responseType, numErrorPoints=10000)
#             print("{} points took {}s".format(numSamples, time.time() - start))
            durations[i] = time.time() - start; l2Errors[i] = l2Error;
            integrals[i] = integral; integralErrors[i] = integralError
    #------------------------------------ save Data ---------------------------------------
    if args.saveFlag == True:
        print("saving Data to {}".format(path))
        if not os.path.exists(path):
            os.makedirs(path)
        # encapuslate all results and the input in 'data' dictionary and save it
        data = {'eigenvalues':eival, 'eigenvectors':eivec, 'sampleRange':sampleRange,
                'durations':durations, 'dim': numDim, 'l2Errors': l2Errors,
                'integrals':integrals, 'integralErrors': integralErrors,
                'model':args.model, 'method':args.method, 'minPoints':args.minPoints,
                'maxPoints':args.maxPoints, 'numSteps':args.numSteps, 'gridType':args.gridType,
                'degree':args.degree, 'responseType':args.responseType, 'asmType':args.asmType,
                'integralType': args.integralType, 'numHistogramMCPoints':numHistogramMCPoints,
                'appSplineLevel': args.appSplineLevel, 'appSplineDegree': args.appSplineDegree,
                'numShadow1DPoints':args.numShadow1DPoints, 'shadow1DEvaluationsArray':shadow1DEvaluationsArray,
                'boundsArray':boundsArray}
        with open(os.path.join(path, 'data.pkl'), 'wb') as fp:
            pickle.dump(data, fp)

