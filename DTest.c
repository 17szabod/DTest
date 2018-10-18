//
// Created by daniel on 9/28/18.
//

#include "DTest.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <python3.7m/Python.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include <libxml/xinclude.h>
// Packages for server-side
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zconf.h>

#define HOSTNAME "localhost"

enum systemTypes {
    Rhino = 0, OpenCasCade = 1, OpenSCAD = 2
};

void recReadXML(Template *out, xmlNodePtr cur_parent, xmlDocPtr doc) {
    xmlNodePtr cur_node = NULL;
    for (cur_node = cur_parent; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            xmlChar *content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            printf("String length %i\n", xmlUTF8Strlen(content));
            printf("node type: %i, name: %s, content: %s\n", cur_node->type, cur_node->name, xmlUTF8Strndup(content, 16));
            if (xmlStrcmp(cur_node->name, BAD_CAST "CAD_System") == 0) {
                out->system = atoi((char *) content);
                xmlFree(content);
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Queries_to_use") == 0) {
                //TODO: Special treatment of queries case
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Bounding_box_coords") == 0) {
                //TODO: Special treatment of bounding box coordinates
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Abs_tol") == 0) {
                out->systemTolerance = strtof(content, NULL);
                xmlFree(content);
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Convexity") == 0) {
                out->convex = atoi((char *) content);
                xmlFree(content);
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Semilocal_simpleconnectivity") == 0) {
                out->semilocallysimplyconnected = (int) strtol((char *) content, NULL, 0);
                xmlFree(content);
            } else if (xmlStrcmp(cur_node->name, BAD_CAST "Semilocal_simpleconnectivity") == 0) {
                out->semilocallysimplyconnected = (int) strtol((char *) content, NULL, 0);
                xmlFree(content);
            }
            recReadXML(out, cur_node->children, doc);
        }
    }
}

Template readTemplate2(char *filename, char *testName) {
    xmlDocPtr doc;
    doc = xmlParseFile(filename);
    if (doc == NULL) {
        fprintf(stderr, "failed to parse the including file\n");
        exit(1);
    }
    Template out;
    xmlNodePtr a_node = xmlDocGetRootElement(doc);
    recReadXML(&out, a_node, doc);

    xmlFreeDoc(doc);

    return out;
}

// source: https://gist.github.com/nolim1t/126991
int socket_connect(char *host, in_port_t port) {
    struct hostent *hp;
    struct sockaddr_in addr;
    int on = 1, sock;

    if ((hp = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        exit(1);
    }
    bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *) &on, sizeof(int));

    if (sock == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(1);

    }
    return sock;
}

Properties startConfigureScript(Template template) {
    Properties prop1;
    if (template.system == OpenCasCade) {
        PyObject *pName, *pModule, *pFunc;
        PyObject *pArgs, *pValue, *pArg1, *pArg2, *pArg3, *pBoundArg;
        Py_Initialize();
        pName = PyUnicode_DecodeFSDefault("/home/daniel/PycharmProjects/ICSI/py_interface.py");
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
                for (int i = 0; i < 2; i++) {
                    for (int j = 0; j < 3; j++) {
                        pBoundArg = PyLong_FromDouble(template.bounds[i][j]);
                        PyTuple_SetItem(pArgs, 3 + i + j, pBoundArg);
                    }
                }
                pValue = PyObject_CallObject(pFunc, pArgs);
                Py_DECREF(pArgs);
                if (pValue != NULL) {
                    PyObject *pSurfAr, *pVol, *pProx;
                    PyObject *pSize;
                    pSurfAr = PyTuple_GetItem(pValue, 0);
                    pVol = PyTuple_GetItem(pValue, 1);
                    pProx = PyTuple_GetItem(pValue, 2);
                    prop1.surfaceArea = (float) PyLong_AsDouble(pSurfAr);
                    printf("Surface Area: %f\n", prop1.surfaceArea);
                    prop1.volume = (float) PyLong_AsDouble(pVol);
                    pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
                    long size = PyLong_AsLong(pSize);
                    double **arr = malloc(size * sizeof(double *));
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
                            Py_DECREF(pArg1);
                            Py_DECREF(pArg2);
                            Py_DECREF(pArg3);
                            Py_DECREF(pos);
                            Py_DECREF(pProx);
                            Py_DECREF(pSize);
                            Py_DECREF(pSurfAr);
                            Py_DECREF(pVol);
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
                    Py_DECREF(pArg1);
                    Py_DECREF(pArg2);
                    Py_DECREF(pArg3);
                    Py_DECREF(pos);
                    Py_DECREF(pProx);
                    Py_DECREF(pSize);
                    Py_DECREF(pSurfAr);
                    Py_DECREF(pVol);
                    Py_DECREF(pBoundArg);
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
        else {
            PyErr_Print();
            fprintf(stderr, "Failed to load \"%s\"\n", template.model);
            exit(1);
        }
        if (Py_FinalizeEx() < 0) {
            fprintf(stderr, "Failed to close python connection\n");
            exit(1);
        }
    } else if (template.system == Rhino) {
        socket_connect(HOSTNAME, 80); // Might need to change ports
    } else if (template.system == OpenSCAD) {

    } else {
        fprintf(stderr, "System not recognized, aborting\n");
        exit(1);
    }
}

int setTolerance(float tol) {
    tolerance = tol;
    return 0;
}

float getTolerance() {
    return tolerance;
}

int performEvaluation(Properties p1, Properties p2, char *testName, Template temp1, Template temp2, double hausdorff) {
    FILE *fp = fopen(testName, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file to perform evaluation for test %s\n", testName);
        exit(1);
    }
    fprintf(fp, "Running test %s on model 1 %s and model 2 %s:\n\n", testName, temp1.model, temp2.model);
    char *systems[3] = {"Rhino", "OpenCasCade", "OpenSCAD"};

    char vol_report[64]; // NOTE: Max buffer size of 64 characters here
    if (fabs(p1.volume - p2.volume) < pow(getTolerance(), 3))
        sprintf(vol_report, "Systems %s and %s have compatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);
    else
        sprintf(vol_report, "Systems %s and %s have incompatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);

    char area_report[64];
    if (fabs(p1.surfaceArea - p2.surfaceArea) < pow(getTolerance(), 2))
        sprintf(area_report, "Systems %s and %s have compatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);
    else
        sprintf(area_report, "Systems %s and %s have incompatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);

    // How exactly are we comparing Hausdorff Distances? Are we looking at the distance between the two proxy models?
    // for now the evaluation just won't make much sense
    char dist_report[64];
    if (fabs(hausdorff) < getTolerance()) //tolerance for hausdorff alg is max(systems)
        sprintf(dist_report, "Systems %s and %s have compatible distances with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], hausdorff);
    else
        sprintf(dist_report, "Systems %s and %s have incompatible distances with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], hausdorff);

    fprintf(fp, "Volume:\n%s\nSurface Area:\n%s\nHausdorff Distance:\n%s", vol_report, area_report, dist_report);
    int fpc = fclose(fp);
    if (fpc != 0) {
        fprintf(stderr, "Failed to close file %s\n", testName);
        return 1;
    }
    return 0;
}


double hausdorff_distance(Properties prop1, Properties prop2) {
    double max_dist = 0;
    double dist;
    double min_dist = LONG_MAX;
    for (int i = 0; i < (int) sizeof(prop1.proxyModel) / sizeof(prop1.proxyModel[0]); i++) {
        for (int j = 0; j < (int) sizeof(prop2.proxyModel) / sizeof(prop2.proxyModel[0]); j++) {
            dist = sqrt(pow((prop1.proxyModel[i][0] - prop2.proxyModel[j][0]), 2) +
                        pow((prop1.proxyModel[i][1] - prop2.proxyModel[j][1]), 2) +
                        pow((prop1.proxyModel[i][2] - prop2.proxyModel[j][2]), 2));
            if (dist < min_dist) min_dist = dist;
        }
        if (min_dist > max_dist) max_dist = min_dist;
    }
    return max_dist;
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
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

    Template temp1 = readTemplate2(file1, test_name);
    Template temp2 = readTemplate2(file2, test_name);
    printf("Template properties:\nSystem: %i\nPMC Precision: %f\n", temp1.system, temp1.algorithmPrecision);

    Properties prop1 = startConfigureScript(temp1);
    Properties prop2 = startConfigureScript(temp2);
    printf("Testing successful property construction:\nSurface Area: %f\nVolume: %f\n", prop1.surfaceArea, prop1.volume);
    double dist = hausdorff_distance(prop1, prop2);


    int eval = performEvaluation(prop1, prop2, test_name, temp1, temp2, dist);
    if (eval != 0) {
        printf("Failed to perform evaluation\n");
        exit(1);
    }
    // Free proxy models of prop1 and prop2
    for (int i = 0; i < (int) sizeof(prop1.proxyModel) / sizeof(prop1.proxyModel[0]); i++) {
        free(prop1.proxyModel[i]);
    }
    free(prop1.proxyModel);
    for (int i = 0; i < (int) sizeof(prop2.proxyModel) / sizeof(prop2.proxyModel[0]); i++) {
        free(prop2.proxyModel[i]);
    }
    free(prop2.proxyModel);
    return 0;
};