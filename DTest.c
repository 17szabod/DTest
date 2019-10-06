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

/**
 * Enumerator for supported system types
 * (Maybe should be in DTest.h)
 */
enum systemTypes {
    Rhino = 0, OpenCasCade = 1, OpenSCAD = 2, MeshLab = 3
};

/**
 * Dumps the contents (not all) of temp
 * @param temp The Template struct to dump
 */
void dump_template(Template temp) {
    printf("\n========DUMPING TEMPLATE FILE=========\n");
    printf("Model: %s\n", temp.model);
    printf("Queries: %lu\n", temp.queries);
    printf("Algorithm Precision: %f\n", temp.algorithmPrecision);
    printf("System: %i\n", temp.system);
    printf("Bounds: ");
    for (int i = 0; i < 6; i++) {
        printf("%f, ", temp.bounds[i > 2 ? 1 : 0][i % 3]);
    }
    printf("\nConvexity: %i\n", temp.convex);
    printf("========END DUMP=========\n\n");
}

/**
 * Dumps the contents of prop
 * @param prop The Properties struct to dump
 */
void dump_properties(Properties prop) {
    printf("\n========DUMPING PROPERTIES FILE=========\n");
    printf("Surface Area: %f\n", prop.surfaceArea);
    printf("Volume: %f\n", prop.volume);
    printf("Number of points: %lu\n", prop.num_points);
    printf("Proxy model is at: %p\n", prop.proxyModel);
    printf("========END DUMP=========\n\n");
}

/**
 * Part of readXML, takes in the system version
 * @param out Template to write to
 * @param sys_root Current XML root node
 * @param doc XML document
 */
void readSysVersion(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
        if (xmlStrEqual(child_node->name, (const xmlChar *) "CAD_System")) {
            out->system = atoi((char *) content);
        }
        child_node = child_node->next;
    }
}

/**
 * Part of readXML, takes in the bounds
 * @param out Template to write to
 * @param sys_root Current XML root node
 * @param doc XML document
 */
void readAABBCoords(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        if (xmlStrEqual(child_node->name, (const xmlChar *) "xmin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][0] = atof((char *) content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "ymin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][1] = atof((char *) content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "zmin")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[0][2] = atof((char *) content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "xmax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][0] = atof((char *) content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "ymax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][1] = atof((char *) content);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "zmax")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->bounds[1][2] = atof((char *) content);
        }
        child_node = child_node->next;
    }
}

/**
 * Unused part of readXML, takes in the list of possible queries and stores them using a bitmask (untested)
 * @param out Template to write to
 * @param sys_root Current XML root node
 * @param doc XML document
 */
void readQueries(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    long bitmask = 0;
    int k = 0;
    while (child_node) {
        bitmask += pow(2, k);
        k += 1;
        child_node = child_node->next;
    }
    out->queries = bitmask;
}

/**
 * Part of readXML, takes in the model info
 * @param out Template to write to
 * @param sys_root Current XML root node
 * @param doc XML document
 */
void readModelInfo(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        if (xmlStrEqual(child_node->name, (const xmlChar *) "FileName")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->model = (char *) content;
        } else if (xmlStrEqual(child_node->name, (const xmlChar *) "Convexity")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->convex = atoi((char *) content);
        } else if (xmlStrEqual(child_node->name, (const xmlChar *) "Semilocal_simpleconnectivity")) {
            xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
            out->semilocallysimplyconnected = (int) strtol((char *) content, NULL, 0);
        }
        child_node = child_node->next;
    }
}

/**
 * Part of readXML, takes in the tolerances
 * @param out Template to write to
 * @param sys_root Current XML root node
 * @param doc XML document
 */
void readTolerances(Template *out, xmlNodePtr sys_root, xmlDocPtr doc) {
    xmlNodePtr child_node = sys_root->children;
    while (child_node) {
        xmlChar *content = xmlNodeListGetString(doc, child_node->xmlChildrenNode, 1);
        if (xmlStrEqual(child_node->name, (const xmlChar *) "Abs_tol")) {
            out->systemTolerance = strtof((char *) content, NULL);
        }
        if (xmlStrEqual(child_node->name, (const xmlChar *) "alg_tol")) {
            out->algorithmPrecision = strtof((char *) content, NULL);
        }
        child_node = child_node->next;
    }
}

/**
 * The main method to read the given template files
 * @param out a reference to a template that will be populated
 * @param cur_parent The root no to read the xml contents under
 * @param doc The actual xml document to read from
 */
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

/**
 * A wrapper for radXML that avoids low level xml control
 * @param filename The name of the xml file
 * @param testName The test name, which currently defaults to the filename
 * @param debug A flag for whether to debug
 * @return A populated Template struct
 */
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

/**
 * A currently unused method to perform network queries
 * @param host
 * @param port
 * @return
 */
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

/**
 * The main call to compute the properties of a template that is an OCC system
 * @param template The given template containing settings etc
 * @param pModule The Python module to connect to
 * @param prop The Properties struct to populate
 * @param debug A flag for debug mode
 * @return 0 for success, 1 for failure
 */
int runOCCConfigure(Template template, PyObject *pModule, Properties *prop, int debug) {
    PyObject * pFunc;
    pFunc = PyObject_GetAttrString(pModule, "occ_configure");
    /* pFunc is a new reference */

    if (pFunc && PyCallable_Check(pFunc)) {

        PyObject * pArgs, *pValue, *pArg1, *pArg2, *pArg3, *pBoundArg;
        if (template.system != OpenCasCade) {
            fprintf(stderr, "Tried to run OCC configure on system %i, which is not OCC.\n", template.system);
            exit(1);
        }
        if (debug) {
            printf("Calling a new round of occ configure\n");
        }
        pArgs = PyTuple_New(9);
        // The arguments will be: Sys_eps, alg_eps, and some access to the shape (filename?)
        // Convert arguments to Python types
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
        // Callthe function (I believe it waits for termination without needing to call Wait(NULL)
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
            // Get arguments back and make them usable
            pSurfAr = PyTuple_GetItem(pValue, 0);
            pVol = PyTuple_GetItem(pValue, 1);
            pProx = PyTuple_GetItem(pValue, 2);
            prop->surfaceArea = PyFloat_AsDouble(pSurfAr);
            if (debug) {
                printf("Surface Area: %f\n", prop->surfaceArea);
            }
            prop->volume = PyFloat_AsDouble(pVol);
            // The point array might be large, so we need to do some extra work
            pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
            unsigned long size = PyLong_AsUnsignedLong(pSize);
            prop->proxyModel = malloc(size * sizeof(double *));
            for (int i = 0; i < size; i++) {
                prop->proxyModel[i] = malloc(4 * sizeof(double)); // Each entry is [x,y,z,rad]
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
            prop->num_points = size;
            // Retrieves all the points
            while (PyDict_Next(pProx, &pos, &key, &value)) {
                xVal = PyTuple_GetItem(key, 0);
                yVal = PyTuple_GetItem(key, 1);
                zVal = PyTuple_GetItem(key, 2);
                x = PyFloat_AsDouble(xVal);
                y = PyFloat_AsDouble(yVal);
                z = PyFloat_AsDouble(zVal);
                if (debug) {
                    printf("Working on point [%f, %f, %f], the value is: %f\n", x, y, z, PyFloat_AsDouble(value));
                }
                if (x == -1 && PyErr_Occurred()) {
                    PyErr_Print();
                    fprintf(stderr, "Failed to read value %i in the proxy model.\n", i);
                    exit(1);
                }
                prop->proxyModel[i][0] = x;
                prop->proxyModel[i][1] = y;
                prop->proxyModel[i][2] = z;
                prop->proxyModel[i][3] = PyFloat_AsDouble(value);
                i++;
            }
            if (debug) {
                printf("Successfully read the proxy model\n");
                printf("Successfully set the proxy model\n");
            }
            if (debug) {
                printf("Successfully populated new properties structure!\n");
                dump_properties(*prop);
            }
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
    Py_DECREF(pFunc);
    return 0;
}


/**
 * The main call to compute the properties of a template that is a SCAD system
 * @param template The given template containing settings etc
 * @param pModule The Python module to connect to
 * @param prop The Properties struct to populate
 * @param debug A flag for debug mode
 * @return 0 for success, 1 for failure
 */
int runSCADConfigure(Template template, PyObject *pModule, Properties *prop, int debug) {
    PyObject * pFunc, *pValue, *pArgs;
    pFunc = PyObject_GetAttrString(pModule, "scad_configure");
    /* pFunc is a new reference */

    if (pFunc && PyCallable_Check(pFunc)) {

        PyObject *pArg1, *pArg2, *pArg3, *pBoundArg;
        if (template.system != OpenSCAD) {
            fprintf(stderr, "Tried to run SCAD configure on system %i, which is not SCAD.\n", template.system);
            exit(1);
        }
        if (debug) {
            printf("Calling a new round of scad configure\n");
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
            prop->surfaceArea = PyFloat_AsDouble(pSurfAr);
            if (debug) {
                printf("Surface Area: %f\n", prop->surfaceArea);
            }
            prop->volume = PyFloat_AsDouble(pVol);
            pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
            unsigned long size = PyLong_AsUnsignedLong(pSize);
            prop->proxyModel = malloc(size * sizeof(double *));
            for (int i = 0; i < size; i++) {
                prop->proxyModel[i] = malloc(4 * sizeof(double));
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
            prop->num_points = size;
            while (PyDict_Next(pProx, &pos, &key, &value)) {
                xVal = PyTuple_GetItem(key, 0);
                yVal = PyTuple_GetItem(key, 1);
                zVal = PyTuple_GetItem(key, 2);
                x = PyFloat_AsDouble(xVal);
                y = PyFloat_AsDouble(yVal);
                z = PyFloat_AsDouble(zVal);
                if (debug) {
                    printf("Working on point [%f, %f, %f], the value is: %f\n", x, y, z, PyFloat_AsDouble(value));
                }
                if (x == -1 && PyErr_Occurred()) {
                    PyErr_Print();
                    fprintf(stderr, "Failed to read value %i in the proxy model.\n", i);
                    exit(1);
                }
                prop->proxyModel[i][0] = x;
                prop->proxyModel[i][1] = y;
                prop->proxyModel[i][2] = z;
                prop->proxyModel[i][3] = PyFloat_AsDouble(value);
                i++;
            }
            if (debug) {
                printf("Successfully read the proxy model\n");
                printf("Successfully set the proxy model\n");
            }
            if (debug) {
                printf("Successfully populated new properties structure!\n");
                dump_properties(*prop);
            }

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
        fprintf(stderr, "Cannot find function \"%s\"\n", "scad_configure");
    }
    return 0;
}


/**
 * The main call to compute the properties of a template that is a MeshLab system
 * @param template The given template containing settings etc
 * @param pModule The Python module to connect to
 * @param prop The Properties struct to populate
 * @param debug A flag for debug mode
 * @return 0 for success, 1 for failure
 */
int runMeshLabConfigure(Template template, PyObject *pModule, Properties *prop, int debug) {
    if (debug) {
        fprintf(stdout, "Starting to run MeshLab Configure.\n");
    }
    PyObject * pFunc;
    pFunc = PyObject_GetAttrString(pModule, "meshlab_configure_cover");
    /* pFunc is a new reference */

    if (pFunc && PyCallable_Check(pFunc)) {
        PyObject * pArgs, *pValue, *pArg1, *pArg2, *pArg3, *pBoundArg;
        if (template.system != MeshLab) {
            fprintf(stderr, "Tried to run MeshLab configure on system %i, which is not MeshLab.\n", template.system);
            exit(1);
        }
        if (debug) {
            printf("Calling a new round of MeshLab configure\n");
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
            prop->surfaceArea = PyFloat_AsDouble(pSurfAr);
            if (debug) {
                printf("Surface Area: %f\n", prop->surfaceArea);
            }
            prop->volume = PyFloat_AsDouble(pVol);
            pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
            unsigned long size = PyLong_AsUnsignedLong(pSize);
            prop->proxyModel = malloc(size * sizeof(double *));
            for (int i = 0; i < size; i++) {
                prop->proxyModel[i] = malloc(4 * sizeof(double));
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
            prop->num_points = size;
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
                prop->proxyModel[i][0] = x;
                prop->proxyModel[i][1] = y;
                prop->proxyModel[i][2] = z;
                prop->proxyModel[i][3] = PyFloat_AsDouble(value);
                i++;
            }
            if (debug) {
                printf("Successfully read the proxy model\n");
                printf("Successfully set the proxy model\n");
            }
            if (debug) {
                printf("Successfully populated new properties sructure!\n");
                dump_properties(*prop);
            }
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
        fprintf(stderr, "Cannot find function \"%s\"\n", "meshlab_configure");
    }
    return 0;
}


/**
 * The main call to compute the properties of a template that is a Rhino system
 * @param template The given template containing settings etc
 * @param pModule The Python module to connect to
 * @param prop The Properties struct to populate
 * @param debug A flag for debug mode
 * @return 0 for success, 1 for failure
 */
int runRhinoConfigure(Template template, PyObject *pModule, Properties *prop, int debug) {
    // For now, this only assumes that the given filename is a cover and does not do complex calculations
    PyObject * pFunc;
    pFunc = PyObject_GetAttrString(pModule, "rhino_configure");
    /* pFunc is a new reference */

    if (pFunc && PyCallable_Check(pFunc)) {

        PyObject * pArgs, *pValue, *pArg1, *pArg2, *pArg3;
        if (template.system != Rhino) {
            fprintf(stderr, "Tried to run rhino configure on system %i, which is not Rhino.\n", template.system);
            exit(1);
        }
        if (debug) {
            printf("Calling a new round of rhino configure\n");
        }
        pArgs = PyTuple_New(3);
        // The arguments will be: Sys_eps, alg_eps, and some access to the shape (filename?)
        pArg1 = PyFloat_FromDouble(template.systemTolerance);
        pArg2 = PyFloat_FromDouble(template.algorithmPrecision);
        pArg3 = PyUnicode_DecodeFSDefault(template.model);
        PyTuple_SetItem(pArgs, 0, pArg1);
        PyTuple_SetItem(pArgs, 1, pArg2);
        PyTuple_SetItem(pArgs, 2, pArg3);

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
            prop->surfaceArea = PyFloat_AsDouble(pSurfAr);
            if (debug) {
                printf("Surface Area: %f\n", prop->surfaceArea);
            }
            prop->volume = PyFloat_AsDouble(pVol);
            pSize = PyLong_FromSsize_t(PyDict_Size(pProx));
            unsigned long size = PyLong_AsUnsignedLong(pSize);
            prop->proxyModel = malloc(size * sizeof(double *));
            for (int i = 0; i < size; i++) {
                prop->proxyModel[i] = malloc(4 * sizeof(double));
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
            prop->num_points = size;
            while (PyDict_Next(pProx, &pos, &key, &value)) {
                xVal = PyTuple_GetItem(key, 0);
                yVal = PyTuple_GetItem(key, 1);
                zVal = PyTuple_GetItem(key, 2);
                x = PyFloat_AsDouble(xVal);
                y = PyFloat_AsDouble(yVal);
                z = PyFloat_AsDouble(zVal);
                if (debug) {
                    printf("Working on point [%f, %f, %f], the value is: %f\n", x, y, z, PyFloat_AsDouble(value));
                }
                if (x == -1 && PyErr_Occurred()) {
                    PyErr_Print();
                    fprintf(stderr, "Failed to read value %i in the proxy model.\n", i);
                    exit(1);
                }
                prop->proxyModel[i][0] = x;
                prop->proxyModel[i][1] = y;
                prop->proxyModel[i][2] = z;
                prop->proxyModel[i][3] = PyFloat_AsDouble(value);
                i++;
            }
            if (debug) {
                printf("Successfully read the proxy model\n");
                printf("Successfully set the proxy model\n");
            }
            if (debug) {
                printf("Successfully populated new properties structure!\n");
                dump_properties(*prop);
            }
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
        fprintf(stderr, "Cannot find function \"%s\"\n", "rhino_configure");
    }
    Py_DECREF(pFunc);
    return 0;

}

/**
 * The wrapper function for the configure phase
 * @param props Pointers to the Properties structs to populate
 * @param template1 The first Template
 * @param template2 The second Template
 * @param debug A flag for debug
 * @return 0 for success, exit on failure
 */
int startConfigureScript(Properties *props[2], Template template1, Template template2, int debug) {
    Properties *prop1 = props[0];
    Properties *prop2 = props[1];
    if (template1.system == OpenCasCade || template2.system == OpenCasCade || template1.system == OpenSCAD ||
        template2.system == OpenSCAD || template1.system == Rhino || template2.system == Rhino) {
        // Create the Python Connection- Janky AF
        PyObject * pName, *pModule;
        size_t stringsize;
        // Forcibly sets the path to include all Python Libraries- would need to be changed on another system
        Py_SetPath(Py_DecodeLocale(
                "/home/daniel/anaconda3/lib/python36.zip:/home/daniel/anaconda3/lib/python3.6:/home/daniel/anaconda3/lib/python3.6/lib-dynload:/home/daniel/anaconda3/lib/python3.6/site-packages",
                &stringsize));
        Py_Initialize();
        // Again, forcibly makes sure we are using the correct Python- was having issues with this,
        // and may have overdone it
        Py_SetPythonHome(Py_DecodeLocale("/home/daniel/anaconda3/lib/python3.6m", &stringsize));
        Py_SetProgramName(Py_DecodeLocale("/home/daniel/anaconda3/bin/python3.6", &stringsize));
        if (debug) {
            printf("The python home: %s\n", Py_EncodeLocale(Py_GetPythonHome(), &stringsize));
            printf("The exec prefix: %s\n", Py_EncodeLocale(Py_GetPrefix(), &stringsize));
            printf("The program name: %s\n", Py_EncodeLocale(Py_GetProgramName(), &stringsize));
            printf("The full paths in the program: %s\n", Py_EncodeLocale(Py_GetPath(), &stringsize));
        }
        // This one is necessary, this is how it knows where your Python scripts are
        PyRun_SimpleString("import sys\n"
                           "sys.path.append(\"/home/daniel/PycharmProjects/ICSI\")\n"
                           "sys.path.append(\"/home/daniel/PycharmProjects/py_oce\")\n");
        // Python is loaded, begin to load the specific module
        pName = PyUnicode_DecodeFSDefault("py_interface");
        pModule = PyImport_Import(pName);
        Py_DECREF(pName);
        if (pModule != NULL) {
            // Call appropriate version of Configure depending on system- order matters!
            if (template1.system == OpenSCAD) {
                if (runSCADConfigure(template1, pModule, prop1, debug) != 0) {
                    fprintf(stderr, "Failed to run SCAD configure for %s\n", template1.model);
                }
            }
            if (template2.system == OpenSCAD) {
                if (runSCADConfigure(template2, pModule, prop2, debug) != 0) {
                    fprintf(stderr, "Failed to run SCAD configure for %s\n", template2.model);
                }
            }
            if (template1.system == OpenCasCade) {
                if (runOCCConfigure(template1, pModule, prop1, debug) != 0) {
                    fprintf(stderr, "Failed to run OCC configure for %s\n", template1.model);
                    exit(1);
                }
            }
            if (template2.system == OpenCasCade) {
                if (runOCCConfigure(template2, pModule, prop2, debug) != 0) {
                    fprintf(stderr, "Failed to run OCC configure for %s\n", template2.model);
                }
            }
            if (template1.system == Rhino) {
                if (runRhinoConfigure(template1, pModule, prop1, debug) != 0) {
                    fprintf(stderr, "Failed to run Rhino configure for %s\n", template1.model);
                }
            }
            if (template2.system == Rhino) {
                if (runRhinoConfigure(template2, pModule, prop2, debug) != 0) {
                    fprintf(stderr, "Failed to run Rhino configure for %s\n", template2.model);
                }
            }
            if (template1.system == MeshLab) {
                if (runMeshLabConfigure(template1, pModule, prop1, debug) != 0) {
                    fprintf(stderr, "Failed to run MeshLab configure for %s\n", template1.model);
                }
            }
            if (template2.system == MeshLab) {
                if (runMeshLabConfigure(template2, pModule, prop2, debug) != 0) {
                    fprintf(stderr, "Failed to run MeshLab configure for %s\n", template2.model);
                }
            }
        } else {
            PyErr_Print();
            fprintf(stderr, "Failed to load py_interface.py\n");
            exit(1);
        }
        if (Py_FinalizeEx() < 0) {
            fprintf(stderr, "Failed to close python connection\n");
            exit(1);
        } else {
            Py_Finalize();
        }
        // If we ever figure out the Rino compute server, changes would come here, as we wouldn't need Python
    } else if (template1.system == Rhino) {
        socket_connect(HOSTNAME, 80); // Might need to change ports
    } else if (template1.system == OpenSCAD || template2.system == OpenSCAD) {

    } else {
        fprintf(stderr, "System not recognized, aborting\n");
        exit(1);
    }
    if (debug) {
        printf("props[0] is point in the method at %p and props[1] at %p\n", props[0], props[1]);
        printf("props[1] has volume: %f\n", props[1]->volume);
    }
    return 0;
}

/**
 * Basic functions to get and set DTest's tolerance
 * @param tol The tolerance to set to
 * @return 0
 */
int setTolerance(float tol) {
    tolerance = tol;
    return 0;
}

/**
 * Gets DTest's tolerance
 * @return tolerance
 */
float getTolerance() {
    return tolerance;
}

/**
 * PerformsThe evaluation phase of DTest
 * @param p1 The first Properties struct
 * @param p2 The second Properties struct
 * @param testName The name of the comparison test, will also be name of output
 * @param temp1 The first Template struct
 * @param temp2 The second Template struct
 * @param hausdorff The hausdorff distance between the two covers
 * @param debug A flag for whether to debug
 * @return 0 for success, 1 for failure
 */
int performEvaluation(Properties p1, Properties p2, char *testName, Template temp1, Template temp2, double hausdorff,
                      int debug) {
    if (debug) {
        printf("Starting to write to output file %s\n.", testName);
    }
    FILE * fp = fopen(testName, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file to perform evaluation for test %s\n", testName);
        exit(1);
    }
    fprintf(fp, "Running test %s on model 1 %s and model 2 %s with tolerance %5f:\n\n", testName, temp1.templateName, temp2.templateName, getTolerance());
    char *systems[4] = {"Rhino", "OpenCasCade", "OpenSCAD", "MeshLab"};

    char vol_report[128]; // NOTE: Max buffer size of 64 characters here
    if (fabs(p1.volume - p2.volume) < pow(getTolerance(), 3))
        sprintf(vol_report, "Systems %s and %s have compatible volumes with a difference of %.8f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);
    else
        sprintf(vol_report, "Systems %s and %s have incompatible volumes with a difference of %.8f\n",
                systems[temp1.system], systems[temp2.system], p1.volume - p2.volume);

    char area_report[128];
    if (fabs(p1.surfaceArea - p2.surfaceArea) < pow(getTolerance(), 2)) // wait for calculations
        sprintf(area_report, "Systems %s and %s have compatible areas with a difference of %.8f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);
    else
        sprintf(area_report, "Systems %s and %s have incompatible areas with a difference of %.8f\n",
                systems[temp1.system], systems[temp2.system], p1.surfaceArea - p2.surfaceArea);

    // How exactly are we comparing Hausdorff Distances? Are we looking at the distance between the two proxy models?
    // for now the evaluation just won't make much sense
    char dist_report[128];
    if (fabs(hausdorff) < getTolerance()) //tolerance for hausdorff alg is max(systems)
        sprintf(dist_report, "Systems %s and %s have a compatible Hausdorff Distance of %.8f\n",
                systems[temp1.system], systems[temp2.system], hausdorff);
    else
        sprintf(dist_report, "Systems %s and %s have an incompatible Hausdorff Distance of %.8f\n",
                systems[temp1.system], systems[temp2.system], hausdorff);

    fprintf(fp,
            "Volume:\n%sVolume of first proxy model: %.8f, volume of second proxy model: %.8f\n\nSurface Area:\n%sSurface area of first proxy model: %.8f, Surface area of second proxy model: %.8f\n\nHausdorff Distance:\n%s",
            vol_report, p1.volume, p2.volume, area_report, p1.surfaceArea, p2.surfaceArea, dist_report);
    int fpc = fclose(fp);
    if (fpc != 0) {
        fprintf(stderr, "Failed to close file %s\n", testName);
        return 1;
    }
    return 0;
}

/**
 * Computes the Hausdorff Distance of the covers included in the two properties files
 * @param prop1 First Properties struct
 * @param prop2 Second Properties struct
 * @param debug A flag for whether to debug
 * @return The Hausdorff distance
 */
double hausdorff_distance(Properties *prop1, Properties *prop2, int debug) {
    if (debug) {
        printf("============STARTING HAUSDORFF DISTANCE CALCULATION===========\n");
    }
    unsigned long n = prop1->num_points;
    unsigned long m = prop2->num_points;
    double max_dist = 0;
    double dist;
    double min_dist;
    for (int i = 0; i < n; i++) {
        min_dist = LONG_MAX;
        for (int j = 0; j < m; j++) {
            dist = sqrt(pow((prop1->proxyModel[i][0] - prop2->proxyModel[j][0]), 2) +
                        pow((prop1->proxyModel[i][1] - prop2->proxyModel[j][1]), 2) +
                        pow((prop1->proxyModel[i][2] - prop2->proxyModel[j][2]), 2));
            if (dist < min_dist) min_dist = dist;
        }
        if (debug) printf("Distance for point %d is %f\n", i, min_dist);
        if (min_dist > max_dist) max_dist = min_dist;
    }
    double max_dist1 = max_dist;
    if (debug) {
        printf("Distance between model 1 and model 2 is %f\n", max_dist1);
    }
    max_dist = 0;
    for (int j = 0; j < m; j++) {
        min_dist = LONG_MAX;
        for (int i = 0; i < n; i++) {
            dist = sqrt(pow((prop1->proxyModel[i][0] - prop2->proxyModel[j][0]), 2) +
                        pow((prop1->proxyModel[i][1] - prop2->proxyModel[j][1]), 2) +
                        pow((prop1->proxyModel[i][2] - prop2->proxyModel[j][2]), 2));
            if (dist < min_dist) min_dist = dist;
        }
        if (debug) printf("Distance for point %d is %f\n", j, min_dist);
        if (min_dist > max_dist) max_dist = min_dist;
    }
    if (debug) {
        printf("Distance between model 2 and model 1 is %f\n", max_dist);
    }
    if (debug) {
        printf("============FINISHED HAUSDORFF DISTANCE CALCULATION===========\n");
    }
    return max_dist > max_dist1 ? max_dist : max_dist1;
}

/**
 * The main file, which intends to be a CLI for the DTest system
 * @param argc arg count
 * @param argv arg vector
 * @return 0 for success, 1 for failure
 */
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

    // Creates and populates the template files
    Template temp1;
    temp1 = readTemplate(file1, test_name, debug);
    temp1.templateName = file1;
    Template temp2;
    temp2 = readTemplate(file2, test_name, debug);
    temp2.templateName = file2;
    setTolerance(temp1.algorithmPrecision + temp2.algorithmPrecision);
    if (debug) {
        dump_template(temp1);
        dump_template(temp2);
    }
    Properties dec_prop1, dec_prop2;
    Properties *props[2];
    props[0] = &dec_prop1;
    props[1] = &dec_prop2;
    // The main part of the code, runs the configure script, passing workload over to Python backend
    startConfigureScript(props, temp1, temp2, debug);
    if (debug) {
        printf("Actually got the properties!!!!!\n");
        printf("props[0] is pointing outside the method at %p and props[1] at %p\n", props[0], props[1]);
        printf("props[1] has volume: %f\n", props[1]->volume);
        dump_properties(*props[1]);
        dump_properties(*props[0]);
    }
    Properties prop1 = *props[0];
    Properties prop2 = *props[1];
    if (debug) {
        printf("Testing successful property construction:\nSurface Area: %f\nVolume: %f\n", prop2.surfaceArea,
               prop2.volume);
    }
    // Computes the Hausdorff distance
    double dist = hausdorff_distance(&prop1, &prop2, debug);
    if (debug) {
        printf("Testing successful hausdorff calculation:\n: %f\n", dist);
    }

    // Performs Evaluation, writing to output test_name
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