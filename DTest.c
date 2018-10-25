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
#include <Python.h>
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


void dump_template(Template temp) {
    printf("\n========DUMPING TEMPLATE FILE=========\n");
    printf("Model: %s\n", temp.model);
    printf("Queries: %lo\n", temp.queries);
    printf("Algorithm Precision: %f\n", temp.algorithmPrecision);
    printf("System: %i\n", temp.system);
    printf("Bounds: ");
    for (int i = 0; i < 6; i++) {
        printf("%f, ", temp.bounds[i > 2 ? 1 : 0][i % 3]);
    }
    printf("\nConvexity: %i\n", temp.convex);
    printf("========END DUMP=========\n\n");
}


void dump_properties(Properties prop) {
    printf("\n========DUMPING PROPERTIES FILE=========\n");
    printf("Surface Area: %f\n", prop.surfaceArea);
    printf("Volume: %f\n", prop.volume);
    printf("Number of points: %lo\n", prop.num_points);
    printf("Size of proxy model: %lo\n", sizeof(prop.proxyModel) / sizeof(prop.proxyModel[0]));
    printf("========END DUMP=========\n\n");
}


void readSysVersion(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
        if (xmlStrEqual(child_node->name, (const xmlChar *) "CAD_System")) {
            out->system = atoi((char *) content);
//            xmlFree(content);
        }
        child_node = child_node->next;
    }
}

void readAABBCoords(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        if (xmlStrEqual(child_node->name, (const xmlChar *) "xmin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][0] = atof((char *) content);
//            xmlFree(content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "ymin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][1] = atof((char *) content);
//            xmlFree(content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "zmin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][2] = atof((char *) content);
//            xmlFree(content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "xmax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][0] = atof((char *) content);
//            xmlFree(content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "ymax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][1] = atof((char *) content);
//            xmlFree(content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "zmax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][2] = atof((char *) content);
//            xmlFree(content);
        }
        child_node = child_node->next;
    }
}

void readQueries(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    long bitmask = 0;
    int k = 0;
    while (child_node) {
//        xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
//        if (atoi((char *) content) == 1) {
        bitmask += pow(2, k);
//        }
        k += 1;
        child_node = child_node->next;
    }
    out->queries = bitmask;
}

void readModelInfo(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        if (xmlStrEqual(child_node->name, (const xmlChar *) "FileName")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->model = (char *) content;
//            xmlFree(content);
        } else if (xmlStrEqual(child_node->name, (const xmlChar *) "Convexity")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->convex = atoi((char *) content);
//            xmlFree(content);
        } else if (xmlStrEqual(child_node->name, (const xmlChar *) "Semilocal_simpleconnectivity")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->semilocallysimplyconnected = (int) strtol((char *) content, NULL, 0);
//            xmlFree(content);
        }
        child_node = child_node->next;
    }
}

void readTolerances(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
        if (xmlStrEqual(child_node->name, (const xmlChar *) "Abs_tol")) {
            out->systemTolerance = strtof((char *) content, NULL);
//            xmlFree(content);
//            child_node = child_node->next;
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "alg_tol")) {
            out->algorithmPrecision = strtof((char *) content, NULL);
//            xmlFree(content);
//            child_node = child_node->next;
        }
        child_node = child_node->next;
    }
}

void readXML(Template *out, xmlNodePtr cur_parent, xmlDocPtr doc, int debug) {
    xmlNodePtr cur_node = cur_parent->children;
    while (cur_node) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (debug) {
                xmlChar *content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
                printf("String length: %i\n", xmlUTF8Strlen(content));
                printf("node type: %i, name: %s, content: %s\n", cur_node->type, cur_node->name,
                       xmlUTF8Strndup(content, 16));
            }
            if (xmlStrEqual(cur_node->name, (const xmlChar *) "CAD_System_and_Version")) {
                readSysVersion(out, cur_node, doc);
            } else if (xmlStrEqual(cur_node->name, (const xmlChar *) "Queries_to_use")) {
                readQueries(out, cur_node, doc);
            } else if (xmlStrEqual(cur_node->name, (const xmlChar *) "Bounding_box_coords")) {
                readAABBCoords(out, cur_node, doc);
            } else if (xmlStrEqual(cur_node->name, (const xmlChar *) "Model_Information")) {
                readModelInfo(out, cur_node, doc);
            } else if (xmlStrEqual(cur_node->name, (const xmlChar *) "Tolerances")) {
                readTolerances(out, cur_node, doc);
            }
        }
        cur_node = cur_node->next;
    }
    if (debug) {
        dump_template(*out);
    }
}

Template readTemplate(char *filename, char *testName, int debug) {
    xmlDocPtr doc;
    doc = xmlParseFile(filename);
    if (doc == NULL) {
        fprintf(stderr, "failed to parse the including file\n");
        exit(1);
    }
    Template out;
    xmlNodePtr a_node = xmlDocGetRootElement(doc);
    readXML(&out, a_node, doc, debug);

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

Properties *startConfigureScript(Template template1, Template template2, int debug) {
    Properties new_properties1;
    Properties *prop1 = &new_properties1;
    Properties new_properties2;
    Properties *prop2 = &new_properties2;
    Properties *props[2];
    props[0] = prop1;
    props[1] = prop2;
    Template templates[2];
    templates[0] = template1;
    templates[1] = template2;
    if (template1.system == OpenCasCade || template2.system == OpenCasCade) {
        PyObject * pName, *pModule, *pFunc;
        size_t stringsize;
        Py_SetPath(Py_DecodeLocale(
                "/home/daniel/anaconda3/lib/python36.zip:/home/daniel/anaconda3/lib/python3.6:/home/daniel/anaconda3/lib/python3.6/lib-dynload:/home/daniel/anaconda3/lib/python3.6/site-packages",
                &stringsize));
        Py_Initialize();
        Py_SetPythonHome(Py_DecodeLocale("/home/daniel/anaconda3/lib/python3.6m", &stringsize));
        Py_SetProgramName(Py_DecodeLocale("/home/daniel/anaconda3/bin/python3.6", &stringsize));
        if (debug) {
            printf("The python home: %s\n", Py_EncodeLocale(Py_GetPythonHome(), &stringsize));
            printf("The exec prefix: %s\n", Py_EncodeLocale(Py_GetPrefix(), &stringsize));
            printf("The program name: %s\n", Py_EncodeLocale(Py_GetProgramName(), &stringsize));
            printf("The full paths in the program: %s\n", Py_EncodeLocale(Py_GetPath(), &stringsize));
        }
        PyRun_SimpleString("import sys\n"
                           "sys.path.append(\"/home/daniel/PycharmProjects/ICSI\")\n"
                           "sys.path.append(\"/home/daniel/PycharmProjects/py_oce\")\n");
//        pName = PyUnicode_DecodeFSDefault("hmm.py");
        pName = PyUnicode_DecodeFSDefault("py_interface");
        pModule = PyImport_Import(pName);
        Py_DECREF(pName);
        if (pModule != NULL) {
            pFunc = PyObject_GetAttrString(pModule, "occ_configure");
            /* pFunc is a new reference */

            if (pFunc && PyCallable_Check(pFunc)) {
                for (int which_template = 0; which_template < 2; which_template++) {
                    PyObject * pArgs, *pValue, *pArg1, *pArg2, *pArg3, *pBoundArg;
                    Template template = templates[which_template];
                    if (template.system != OpenCasCade) {
                        continue;
                    }
                    if (debug) {
                        printf("Calling a new round of occ configure\n");
                    }
                    pArgs = PyTuple_New(9);
                    // The arguments will be: Sys_eps, alg_eps, and some access to the shape (filename?)
                    pArg1 = PyFloat_FromDouble(template.systemTolerance);
                    pArg2 = PyFloat_FromDouble(template.algorithmPrecision);
                    pArg3 = PyUnicode_DecodeFSDefault(template.model);
                    PyTuple_SetItem(pArgs, 0, pArg1);
                    PyTuple_SetItem(pArgs, 1, pArg2);
                    PyTuple_SetItem(pArgs, 2, pArg3);
                    for (int i = 0; i < 2; i++) {
                        for (int j = 0; j < 3; j++) {
                            pBoundArg = PyFloat_FromDouble(template.bounds[i][j]);
                            PyTuple_SetItem(pArgs, 3 + 3 * i + j, pBoundArg);
                        }
                    }
                    pValue = PyTuple_New(3);
                    pValue = PyObject_CallObject(pFunc, pArgs);
                    if (pValue != NULL) {
                        PyObject * pSurfAr, *pVol, *pProx;
                        PyObject * pSize;
                        if (!PyTuple_CheckExact(pValue)) {
                            fprintf(stderr, "Did not receive a tuple from function call, exiting.\n");
                            exit(1);
                        }
                        if (debug) {
                            printf("The length of the tuple: %lo\n", PyTuple_Size(pValue));
                        }
                        pSurfAr = PyTuple_GetItem(pValue, 0);
                        pVol = PyTuple_GetItem(pValue, 1);
                        pProx = PyTuple_GetItem(pValue, 2);
                        props[which_template]->surfaceArea = PyFloat_AsDouble(pSurfAr);
                        if (debug) {
                            printf("Surface Area: %f\n", props[which_template]->surfaceArea);
                        }
                        props[which_template]->volume = PyFloat_AsDouble(pVol);
                        pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
                        long size = PyLong_AsLong(pSize);
                        props[which_template]->proxyModel = malloc(size * sizeof(double *));
                        for (int i = 0; i < size; i++) {
                            props[which_template]->proxyModel[i] = malloc(4 * sizeof(double));
                        }
                        if (debug) {
                            printf("Successfully allocated space for proxy model\n");
                        }
                        PyObject * key, *value;
                        PyObject * xVal, *yVal, *zVal;
                        double x, y, z;
                        int i = 0;
                        Py_ssize_t
                        pos = 0;
                        if (debug) {
                            printf("Starting to loop through each element in dictionary\n");
                            printf("The size of the dictionary is: %lo\n", size);
                        }
                        props[which_template]->num_points = size;
                        while (PyDict_Next(pProx, &pos, &key, &value)) {
                            xVal = PyTuple_GetItem(key, 0);
                            yVal = PyTuple_GetItem(key, 1);
                            zVal = PyTuple_GetItem(key, 2);
                            x = PyFloat_AsDouble(xVal);
                            y = PyFloat_AsDouble(yVal);
                            z = PyFloat_AsDouble(zVal);
                            if (debug) {
                                printf("Working on point [%f, %f, %f]\n", x, y, z);
                            }
                            if (x == -1 && PyErr_Occurred()) {
                                PyErr_Print();
                                fprintf(stderr, "Failed to read value %i in the proxy model.\n", i);
                                exit(1);
                            }
                            props[which_template]->proxyModel[i][0] = x;
                            props[which_template]->proxyModel[i][1] = y;
                            props[which_template]->proxyModel[i][2] = z;
                            props[which_template]->proxyModel[i][3] = PyFloat_AsDouble(value);
                            i++;
                        }
                        if (debug) {
                            printf("Successfully read the proxy model\n");
                            printf("Successfully set the proxy model\n");
                        }
                        if (debug) {
                            printf("Successfully populated new properties sructure!\n");
                        }
                    } else {
                        Py_DECREF(pFunc);
                        Py_DECREF(pModule);
                        PyErr_Print();
                        fprintf(stderr, "Call failed\n");
                        exit(1);
                    }
                }
            } else {
                if (PyErr_Occurred())
                    PyErr_Print();
                fprintf(stderr, "Cannot find function \"%s\"\n", "occ_configure");
            }
        } else {
            PyErr_Print();
            fprintf(stderr, "Failed to load py_interface.py\n");
            exit(1);
        }
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        if (Py_FinalizeEx() < 0) {
            fprintf(stderr, "Failed to close python connection\n");
            exit(1);
        } else {
            Py_Finalize();
        }
    } else if (template1.system == Rhino) {
        socket_connect(HOSTNAME, 80); // Might need to change ports
    } else if (template1.system == OpenSCAD || template2.system == OpenSCAD) {

    } else {
        fprintf(stderr, "System not recognized, aborting\n");
        exit(1);
    }
    return *props;
}

int setTolerance(float tol) {
    tolerance = tol;
    return 0;
}

float getTolerance() {
    return tolerance;
}

int performEvaluation(Properties p1, Properties p2, char *testName, Template temp1, Template temp2, double hausdorff, int debug) {
    if (debug) {
        printf("Starting to write to output file %s\n.", testName);
    }
    FILE * fp = fopen(testName, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file to perform evaluation for test %s\n", testName);
        exit(1);
    }
    fprintf(fp, "Running test %s on model 1 %s and model 2 %s:\n\n", testName, temp1.model, temp2.model);
    char *systems[3] = {"Rhino", "OpenCasCade", "OpenSCAD"};

    char vol_report[128]; // NOTE: Max buffer size of 64 characters here
    if (fabs(p1.volume - p2.volume) < pow(getTolerance(), 3))
        sprintf(vol_report, "Systems %s and %s have compatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);
    else
        sprintf(vol_report, "Systems %s and %s have incompatible volumes with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);

    char area_report[128];
    if (fabs(p1.surfaceArea - p2.surfaceArea) < pow(getTolerance(), 2))
        sprintf(area_report, "Systems %s and %s have compatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);
    else
        sprintf(area_report, "Systems %s and %s have incompatible areas with a difference of %f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);

    // How exactly are we comparing Hausdorff Distances? Are we looking at the distance between the two proxy models?
    // for now the evaluation just won't make much sense
    char dist_report[128];
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


double hausdorff_distance(Properties prop1, Properties prop2, int debug) {
    if (debug) {
        printf("============STARTING HAUSDORFF DISTANCE CALCULATION===========\n");
    }
    double max_dist = 0;
    double dist;
    double min_dist = LONG_MAX;
    for (int i = 0; i < prop1.num_points; i++) {
        for (int j = 0; j < prop2.num_points; j++) {
            dist = sqrt(pow((prop1.proxyModel[i][0] - prop2.proxyModel[j][0]), 2) +
                        pow((prop1.proxyModel[i][1] - prop2.proxyModel[j][1]), 2) +
                        pow((prop1.proxyModel[i][2] - prop2.proxyModel[j][2]), 2));
            if (debug) {
                printf("Working on point 1 at [%f, %f, %f] and point 2 at [%f, %f, %f] with a distance of %f.\n",
                       prop1.proxyModel[i][0], prop2.proxyModel[i][1], prop1.proxyModel[i][2], prop2.proxyModel[j][0],
                       prop1.proxyModel[j][1], prop2.proxyModel[j][2], dist);
            }
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
    int debug = FALSE;
    if (*endptr != '\0') {
        fprintf(stderr, "Failed to read tolerance, only got %f\n", tol);
        exit(1);
    }
    if (debug) {
        printf("System tolerance is: %f\n", tol);
    }
    setTolerance(tol);

    Template temp1 = readTemplate(file1, test_name, debug);
    Template temp2 = readTemplate(file2, test_name, debug);
    if (debug) {
        dump_template(temp1);
    }
    Properties *props;
    props = startConfigureScript(temp1, temp2, debug);
    if (debug) {
        printf("Actually got the properties!!!!!\n");
        dump_properties(*props);
        dump_properties(*(props + 1));
    }
    Properties prop1 = *props;
    Properties prop2 = *(props + 1);
    if (debug) {
        printf("Testing successful property construction:\nSurface Area: %f\nVolume: %f\n", prop1.surfaceArea,
               prop1.volume);
    }
    double dist = hausdorff_distance(prop1, prop2, debug);
    if (debug) {
        printf("Testing successful hausdorff calculation:\n: %f\n", dist);
    }


    int eval = performEvaluation(prop1, prop2, test_name, temp1, temp2, dist, debug);
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