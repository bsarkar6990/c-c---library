#include <jni.h>
#include "CL/cl.h"
#include <stdio.h>
#include "JOCLApp1.h"
#include <time.h>
#include <vector>
#include <string>
#include <sstream>
#include <sys/time.h>
#include <math.h>

struct OpenCLObjects
{
    // Regular OpenCL objects:
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;

    // Objects that are specific for this sample.
    bool isInputBufferInitialized;
    cl_mem d_a;
    cl_mem d_b;
    cl_mem d_c;
};
OpenCLObjects openCLObjects;

JNIEXPORT jstring JNICALL Java_JOCLApp1_callGPU
  (JNIEnv *env, jobject thisobj){
	   jstring result;
	  std::string f_str="";
	cl_uint num_of_platforms = 0;
	cl_int err = CL_SUCCESS;
	cl_device_type primary_device_type=CL_DEVICE_TYPE_GPU;
	
	// Get total number of the available platforms.
    clGetPlatformIDs(0, 0, &num_of_platforms);
	printf("\nNumber of Platform %x\n",num_of_platforms);
   
	using namespace std;
	openCLObjects.isInputBufferInitialized = false;
	
	vector<cl_platform_id> platforms(num_of_platforms);
	// Get IDs for all platforms.
    clGetPlatformIDs(num_of_platforms, &platforms[0], 0);
	string required_platform_subname = ""; // e.g. assign to "Intel"
	size_t platform_name_length = 0;
    clGetPlatformInfo(
            platforms[0],
            CL_PLATFORM_NAME,
            0,
            0,
            &platform_name_length
    );
	
	vector<char> platform_name_buffer(platform_name_length);
    clGetPlatformInfo(
            platforms[0],
            CL_PLATFORM_NAME,
            platform_name_length,
            &platform_name_buffer[0],
            0
    );
    char* platform_name = &platform_name_buffer[0];
    printf("Platform name: %s\n",platform_name);
    string selection_marker;
    cl_uint num_devices = 0;
    cl_device_id* devices;
    clGetDeviceIDs(platforms[0], primary_device_type, 0, NULL, &num_devices);
    devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
    clGetDeviceIDs(platforms[0], primary_device_type, num_devices, devices, NULL);
    char* value;
    size_t msglen;
    for (int j = 0; j < num_devices; j++) {
        clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &msglen);
        value = (char*) malloc(msglen);
        clGetDeviceInfo(devices[j], CL_DEVICE_NAME, msglen, value, NULL);
        printf("OpenCL Device name: %s\n",value);
        free(value);
    }
	
	openCLObjects.platform = platforms[0];
    cl_context_properties context_props[] = {
            CL_CONTEXT_PLATFORM,
            cl_context_properties(openCLObjects.platform),
            0
    };
    openCLObjects.context =
            clCreateContextFromType
                    (
                            context_props,
                            primary_device_type,
                            0,
                            0,
                            &err
                    );
    clGetContextInfo
            (
                    openCLObjects.context,
                    CL_CONTEXT_DEVICES,
                    sizeof(openCLObjects.device),
                    &openCLObjects.device,
                    0
            );

    const char* openCLProgramTextNative = "                   \n" \
"__kernel void vecAdd(  __global float *a,                       \n" \
"                       __global float *b,                       \n" \
"                       __global float *c,                       \n" \
"                       const unsigned int n)                    \n" \
"{                                                               \n" \
"    //Get our global thread ID                                  \n" \
"    int id = get_global_id(0);                                  \n" \
"                                                                \n" \
"    //Make sure we do not go out of bounds                      \n" \
"    if (id < n)                                                 \n" \
"        c[id] = sin(a[id])*sin(a[id])*sin(a[id]) + cos(b[id])*cos(b[id])*cos(b[id]);                                  \n" \
"}                                                               \n" \
                                                                "\n" ;
	openCLObjects.program =
            clCreateProgramWithSource
                    (
                            openCLObjects.context,
                            1,
                            &openCLProgramTextNative,
                            0,
                            &err
                    );

    err =clBuildProgram(openCLObjects.program, 0, 0, 0, 0, 0);

    if(err == CL_BUILD_PROGRAM_FAILURE)
    {
        size_t log_length = 0;
        err = clGetProgramBuildInfo(
                openCLObjects.program,
                openCLObjects.device,
                CL_PROGRAM_BUILD_LOG,
                0,
                0,
                &log_length
        );

        vector<char> log(log_length);

        err = clGetProgramBuildInfo(
                openCLObjects.program,
                openCLObjects.device,
                CL_PROGRAM_BUILD_LOG,
                log_length,
                &log[0],
                0
        );

        printf
        (
                "Error happened during the build of OpenCL program.\nBuild log:%s",
                &log[0]
        );

    }
    openCLObjects.kernel = clCreateKernel(openCLObjects.program, "vecAdd", &err);


    cl_queue_properties prop[] = {0};
    openCLObjects.queue =
            clCreateCommandQueueWithProperties
                    (
                            openCLObjects.context,
                            openCLObjects.device,
                            prop,    // Creating queue properties, refer to the OpenCL specification for details.
                            &err
                    );

    // Length of vectors
    long n = 999999;
    // Host input vectors
    float *h_a;
    float *h_b;
    // Host output vector
    float *h_c;
    // Size, in bytes, of each vector
    size_t bytes = n*sizeof(float);

    // Allocate memory for each vector on host
    h_a = (float*)malloc(bytes);
    h_b = (float*)malloc(bytes);
    h_c = (float*)malloc(bytes);
    // Initialize vectors on host
    long i;
    for( i = 0; i < n; i++ )
    {
        h_a[i] = i;
        h_b[i] = i;
    }

    timeval start;


    gettimeofday(&start, NULL);
    size_t globalSize, localSize;
    // Number of work items in each local work group
    localSize = 64;

    // Number of total work items - localSize must be devisor
    globalSize = ceil(n/(float)localSize)*localSize;

    openCLObjects.d_a =
            clCreateBuffer
                    (
                            openCLObjects.context,
                            CL_MEM_READ_ONLY| CL_MEM_USE_HOST_PTR,
                            bytes,   // Buffer size in bytes.
                            h_a,  // Bytes for initialization.
                            &err
                    );
    openCLObjects.d_b =
            clCreateBuffer
                    (
                            openCLObjects.context,
                            CL_MEM_READ_ONLY| CL_MEM_USE_HOST_PTR,
                            bytes,   // Buffer size in bytes.
                            h_b,  // Bytes for initialization.
                            &err
                    );
    openCLObjects.d_c =
            clCreateBuffer
                    (
                            openCLObjects.context,
                            CL_MEM_WRITE_ONLY| CL_MEM_USE_HOST_PTR,
                            bytes,   // Buffer size in bytes.
                            h_c,  // Bytes for initialization.
                            &err
                    );
    err  = clSetKernelArg(openCLObjects.kernel, 0, sizeof(cl_mem), &openCLObjects.d_a);
    err |= clSetKernelArg(openCLObjects.kernel, 1, sizeof(cl_mem), &openCLObjects.d_b);
    err |= clSetKernelArg(openCLObjects.kernel, 2, sizeof(cl_mem), &openCLObjects.d_c);
    err |= clSetKernelArg(openCLObjects.kernel, 3, sizeof(long), &n);

    err = clEnqueueNDRangeKernel(openCLObjects.queue, openCLObjects.kernel, 1, NULL, &globalSize, &localSize,
                                 0, NULL, NULL);

    clEnqueueMapBuffer
            (
                    openCLObjects.queue,
                    openCLObjects.d_c,
                    CL_TRUE,    // to use the host pointer in the next call
                    CL_MAP_READ,
                    0,
                    bytes,
                    0, 0, 0,
                    &err
            );
    err = clEnqueueUnmapMemObject
            (
                    openCLObjects.queue,
                    openCLObjects.d_c,
                    h_c,
                    0, 0, 0
            );
    err = clFinish(openCLObjects.queue);
    timeval tend;
    gettimeofday(&tend,NULL);
    float ndrangeDuration =
            (tend.tv_sec + tend.tv_usec * 1e-6) - (start.tv_sec + start.tv_usec * 1e-6);
    printf("NDRangeKernel time: %f\n", ndrangeDuration);
    double sum = 0;
    timeval tstart1,tend1;
    gettimeofday(&tstart1,NULL);
    for(i=0; i<n; i++) {
        h_c[i]=sin(h_a[i])*sin(h_a[i])*sin(h_a[i])+cos(h_b[i])*cos(h_b[i])*cos(h_b[i]);
    }
    gettimeofday(&tend1,NULL);
    float ndrangeDuration1 =
            (tend1.tv_sec + tend1.tv_usec * 1e-6) - (tstart1.tv_sec + tstart1.tv_usec * 1e-6);
    f_str="GPU time: "+std::to_string(ndrangeDuration)+"second(s), CPU time: "+std::to_string(ndrangeDuration1)+"second(s)";
    clReleaseMemObject(openCLObjects.d_a);
    clReleaseMemObject(openCLObjects.d_b);
    clReleaseMemObject(openCLObjects.d_c);
    clReleaseKernel(openCLObjects.kernel);
    clReleaseProgram(openCLObjects.program);
    clReleaseCommandQueue(openCLObjects.queue);
    clReleaseContext(openCLObjects.context);

	
	  result = env->NewStringUTF(f_str.c_str());
   return result;
  }