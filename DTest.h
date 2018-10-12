//
// Created by daniel on 9/27/18.
//

#ifndef DTEST_DTEST_H
#define DTEST_DTEST_H


typedef struct {
    int system;
    double systemTolerance;
    float algorithmPrecision;
    int queries; // The queries supported will be represented by a bitmask-- we will define a set of all possible
    // queries (less than 32), and the bitmask will represent a subset that is supported.
    int manifold;
    int connected;
    int semilocallysimplyconnected; //added, Duygu
    int closed; //added, Duygu
    int orientation; //added, Duygu
    int convex;
    float minimumFeatureSize;
    float bounds[2][3]; //added, Daniel  form: [[xmin, ymin, zmin], [xmax, ymax, zmax]]
    char *model; // A way to access the model, whether it be a filename or a unique id in a database (filename for now)
} Template;

typedef struct {
    float surfaceArea;
    float volume;
    double **proxyModel;  // A 2D, n by 4 array representing a union of balls -- Should we put this in Properties??? -Yes, Duygu
    // NOTE: proxyModel, because of its size, is on the heap, so free it after use
} Properties;

float tolerance;

Template readTemplate(char* filename, char* testName);// what is testName?, Duygu
Template *readTemplate2(char* filename, char* testName);// what is testName?, Duygu

int setTolerance(float tol);

float getTolerance();

Properties startConfigureScript(Template template);

int performEvaluation(Properties p1, Properties p2, char* testName, Template temp1, Template temp2, double hausdorff);


#endif //DTEST_DTEST_H
