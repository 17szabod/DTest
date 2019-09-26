//
// Created by daniel on 9/27/18.
//

#ifndef DTEST_DTEST_H
#define DTEST_DTEST_H


typedef struct {
    int system;
    double systemTolerance;
    float algorithmPrecision;
    long queries; // The queries supported will be represented by a bitmask-- we will define a set of all possible
    // queries (less than 32), and the bitmask will represent a subset that is supported.
    int manifold;
    int connected;
    int semilocallysimplyconnected; //added, Duygu
    int closed; //added, Duygu
    int orientation; //added, Duygu
    int convex;
    float minimumFeatureSize;
    double bounds[2][3]; //added, Daniel  form: [[xmin, ymin, zmin], [xmax, ymax, zmax]]
    char *model; // A way to access the model, whether it be a filename or a unique id in a database (filename for now)
    char *templateName;
} Template;

typedef struct {
    double surfaceArea;
    double volume;
    long num_points;
    double **proxyModel;  // A 2D, n by 4 array representing a union of balls
    // NOTE: proxyModel, because of its size, is on the heap, so free it after use
} Properties;

float tolerance;

Template readTemplate(char *filename, char *testName, int debug);// what is testName?, Duygu

int setTolerance(float tol);

float getTolerance();

int startConfigureScript(Properties *props[2], Template template1, Template template2, int debug);

int performEvaluation(Properties p1, Properties p2, char* testName, Template temp1, Template temp2, double hausdorff, int debug);


#endif //DTEST_DTEST_H
