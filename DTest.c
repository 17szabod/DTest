//
// Created by daniel on 9/28/18.
//

#include "DTest.h"
#include <stdio.h>
#include <stdlib.h>
#include <python3.6/Python.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>

enum systemTypes {
    Rhino = 0, OpenCasCade = 1, OpenSCAD = 2
};

Template readTemplate(char *filename, char *testName) {
    FILE *fp = fopen(filename, "r");
    Template out;
    int ifp;
    ifp = fscanf(fp, "%f\n%i\n%i\n%i\n%f\n%i\n%f\n%i\n%s", &out.algorithmPrecision, &out.connected, &out.convex,
                 &out.manifold, &out.minimumFeatureSize, &out.queries, &out.systemTolerance, &out.system, out.model);
    if (ifp >= 0) {
        fprintf(stderr, "Failed to read file with name: %s", filename);
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    return out;
}

Template readTemplate2(char *filename, char *testName) {
    xmlDocPtr doc;
    doc = xmlReadFile(filename, NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "failed to parse the including file\n");
        exit(1);
    }
    Template out;
    xmlFreeDoc(doc);
    return out;
}

Properties startConfigureScript(Template template) {
    Properties prop1;
    if (template.system == OpenCasCade) {
        PyObject *pName, *pModule, *pFunc;
        PyObject *pArgs, *pValue, *pArg1, *pArg2, *pArg3;
        Py_Initialize();
        pName = PyUnicode_DecodeFSDefault("py_interface.py");
        pModule = PyImport_Import(pName);
        Py_DECREF(pName);
        if (pModule != NULL) {
            pFunc = PyObject_GetAttrString(pModule, "occ_configure");
            /* pFunc is a new reference */

            if (pFunc && PyCallable_Check(pFunc)) {
                pArgs = PyTuple_New(3);
                // The arguments will be: Sys_eps, alg_eps, and some access to the shape (filename?)
                pArg1 = PyLong_FromDouble(template.systemTolerance);
                pArg2 = PyLong_FromDouble(template.algorithmPrecision);
                pArg3 = PyUnicode_DecodeFSDefault(template.model);
                PyTuple_SetItem(pArgs, 0, pArg1);
                PyTuple_SetItem(pArgs, 1, pArg2);
                PyTuple_SetItem(pArgs, 2, pArg3);
                pValue = PyObject_CallObject(pFunc, pArgs);
                Py_DECREF(pArgs);
                if (pValue != NULL) {
                    PyObject *pHaussDist, *pSurfAr, *pVol, *pProx;
                    PyObject *pSize;
                    pHaussDist = PyTuple_GetItem(pValue, 0);
                    pSurfAr = PyTuple_GetItem(pValue, 1);
                    pVol = PyTuple_GetItem(pValue, 2);
                    pProx = PyTuple_GetItem(pValue, 3);
                    prop1.hausdorffDistance = (float) PyLong_AsDouble(pHaussDist);
                    prop1.surfaceArea = (float) PyLong_AsDouble(pSurfAr);
                    prop1.volume = (float) PyLong_AsDouble(pVol);
                    pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
                    long size = PyLong_AsLong(pSize);
                    double **arr = malloc(size * sizeof(double*));
                    for (int i = 0; i < size; i++) {
                        arr[i] = malloc(4 * sizeof(double));
                    }
                    PyObject *key, *value;
                    PyObject *xVal, *yVal, *zVal;
                    double x, y, z;
                    int i = 0;
                    Py_ssize_t pos = 0;
                    while (PyDict_Next(pProx, &pos, &key, &value)) {
                        xVal = PyTuple_GetItem(value, 0);
                        yVal = PyTuple_GetItem(value, 1);
                        zVal = PyTuple_GetItem(value, 2);
                        x = PyLong_AsDouble(xVal);
                        y = PyLong_AsDouble(yVal);
                        z = PyLong_AsDouble(zVal);
                        if (x == -1 && PyErr_Occurred()) {
                            Py_DECREF(pArg1); Py_DECREF(pArg2); Py_DECREF(pArg3); Py_DECREF(pHaussDist); Py_DECREF(pos);
                            Py_DECREF(pProx); Py_DECREF(pSize); Py_DECREF(pSurfAr); Py_DECREF(pVol);
                            Py_DECREF(pModule);
                            PyErr_Print();
                            fprintf(stderr, "Failed to read value %i in the proxy model.\n", i);
                            exit(1);
                        }
                        arr[i][0] = x;
                        arr[i][1] = y;
                        arr[i][2] = z;
                        arr[i][3] = PyLong_AsDouble(value);
                        i++;
                    }
                    prop1.proxyModel = arr;
                    Py_DECREF(pArg1); Py_DECREF(pArg2); Py_DECREF(pArg3); Py_DECREF(pHaussDist); Py_DECREF(pos);
                    Py_DECREF(pProx); Py_DECREF(pSize); Py_DECREF(pSurfAr); Py_DECREF(pVol);
                    Py_XDECREF(pFunc);
                    Py_DECREF(pModule);
                    return prop1;
                } else {
                    Py_DECREF(pFunc);
                    Py_DECREF(pModule);
                    PyErr_Print();
                    fprintf(stderr, "Call failed\n");
                    exit(1);
                }
            } else {
                if (PyErr_Occurred())
                    PyErr_Print();
                fprintf(stderr, "Cannot find function \"%s\"\n", "occ_configure");
            }
        }
        if (Py_FinalizeEx() < 0) {
            fprintf(stderr, "Failed to close python connection\n");
            exit(1);
        }
    }
    else if (template.system == Rhino) {

    }
    else if (template.system == OpenSCAD) {

    }
    else
        fprintf(stderr, "System not recognized, aborting\n");
    exit(1);
}

int setTolerance(float tol) {
    tolerance = tol;
    return 0;
}

float getTolerance() {
    return tolerance;
}

int performEvaluation(Properties p1, Properties p2, char* testName, Template temp1, Template temp2) {
    FILE *fp = fopen(testName, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file to perform evaluation for test %s\n", testName);
        exit(1);
    }
    fprintf(fp, "Running test %s on model 1 %s and model 2 %s:\n\n", testName, temp1.model, temp2.model);
    char *systems[3] = {"Rhino", "OpenCasCade", "OpenSCAD"};

    char vol_report[64]; // NOTE: Max buffer size of 64 characters here
    if (__fabs(p1.volume - p2.volume) < pow(getTolerance(), 3))
        sprintf(vol_report, "Systems %s and %s have compatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);
    else
        sprintf(vol_report, "Systems %s and %s have incompatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);

    char area_report[64];
    if (__fabs(p1.surfaceArea - p2.surfaceArea) < pow(getTolerance(), 2))
        sprintf(area_report, "Systems %s and %s have compatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);
    else
        sprintf(area_report, "Systems %s and %s have incompatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);

    // How exactly are we comparing Hausdorff Distances? Are we looking at the distance between the two proxy models?
    // for now the evaluation just won't make much sense
    char dist_report[64];
    if (__fabs(p1.hausdorffDistance - p2.hausdorffDistance) < getTolerance())
        sprintf(dist_report, "Systems %s and %s have compatible distances with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.hausdorffDistance - p2.hausdorffDistance);
    else
        sprintf(dist_report, "Systems %s and %s have incompatible distances with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.hausdorffDistance - p2.hausdorffDistance);

    fprintf(fp, "Volume:\n%s\nSurface Area:\n%s\nHausdorff Distance:\n%s", vol_report, area_report, dist_report);
    int fpc = fclose(fp);
    if (fpc != 0) {
        fprintf(stderr, "Failed to close file %s\n", testName);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./DTest <TemplateFile1> <TemplateFile2> <TestName> <Tolerance>\n");
        exit(1);
    }
    char *file1 = argv[1];
    char *file2 = argv[2];
    char *test_name = argv[3];
    char *endptr;
    float tol = strtof(argv[4], &endptr);

    if (*endptr != '\0') {
        fprintf(stderr, "Failed to read tolerance, only got %f\n", tol);
        exit(1);
    }

    setTolerance(tol);

    Template temp1 = readTemplate(file1, test_name);
    Template temp2 = readTemplate(file2, test_name);

    Properties prop1 = startConfigureScript(temp1);
    Properties prop2 = startConfigureScript(temp2);

    int eval = performEvaluation(prop1, prop2, test_name, temp1, temp2);
    if (eval != 0) {
        printf("Failed to perform evaluation\n");
        exit(1);
    }
    // Free proxy models of prop1 and prop2
    for (int i = 0; i < (int)sizeof(prop1.proxyModel) / sizeof(prop1.proxyModel[0]); i++) {
        free(prop1.proxyModel[i]);
    }
    free(prop1.proxyModel);
    for (int i = 0; i < (int)sizeof(prop2.proxyModel) / sizeof(prop2.proxyModel[0]); i++) {
        free(prop2.proxyModel[i]);
    }
    free(prop2.proxyModel);
    return 0;
};