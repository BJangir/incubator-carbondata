/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/time.h>
#include "../src/CarbonReader.h"
#include "../src/CarbonRow.h"
#include "../src/CarbonWriter.h"
#include "../src/CarbonSchemaReader.h"
#include "../src/Schema.h"
#include "../src/CarbonProperties.h"
#include "gtest/gtest.h"

using namespace std;

JavaVM *jvm;
JNIEnv *env;
int my_argc;
char** my_argv;

/**
 * init jvm
 *
 * @return
 */
JNIEnv *initJVM() {
    JNIEnv *env;
    JavaVMInitArgs vm_args;
    int parNum = 2;
    int res;
    JavaVMOption options[parNum];

    options[0].optionString = "-Djava.class.path=../../sdk/target/carbondata-sdk.jar";
    options[1].optionString = "-verbose:jni";                // For debug and check the jni information
    //    options[2].optionString = "-Xmx12000m";            // change the jvm max memory size
    //    options[3].optionString = "-Xms5000m";             // change the jvm min memory size
    //    options[4].optionString = "-Djava.compiler=NONE";  // forbidden JIT
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = parNum;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;

    res = JNI_CreateJavaVM(&jvm, (void **) &env, &vm_args);
    if (res < 0) {
        fprintf(stderr, "\nCan't create Java VM\n");
        exit(1);
    }

    return env;
}


/**
 * convert Exception to String(Exception toString())
 * @param jthrowable exc
 * @return exception as String
 */
 char* convertExceptionToString(jthrowable exc){
        jboolean isCopy = false;
        jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
        jstring s = (jstring) env->CallObjectMethod(exc, toString);
        char *exception_string = (char *) env->GetStringUTFChars(s, JNI_FALSE);
        return exception_string;

    }

/**
 * print array result
 *
 * @param env JNIEnv
 * @param arr array
 */
void printArray(JNIEnv *env, jobjectArray arr) {
    if (env->ExceptionCheck()) {
        throw env->ExceptionOccurred();
    }
    jsize length = env->GetArrayLength(arr);
    int j = 0;
    for (j = 0; j < length; j++) {
        jobject element = env->GetObjectArrayElement(arr, j);
        if (env->ExceptionCheck()) {
            throw env->ExceptionOccurred();
        }
        char *str = (char *) env->GetStringUTFChars((jstring) element, JNI_FALSE);
        printf("%s\t", str);
    }
    env->DeleteLocalRef(arr);
}

/**
 * print boolean result
 *
 * @param env JNIEnv
 * @param bool1 boolean value
 */
void printBoolean(jboolean bool1) {
    if (bool1) {
        printf("true\t");
    } else {
        printf("false\t");
    }
}

/**
 * print result of reading data
 *
 * @param env JNIEnv
 * @param reader CarbonReader object
 */
void printResultWithException(JNIEnv *env, CarbonReader reader) {
    CarbonRow carbonRow(env);
    try {
        while (reader.hasNext()) {
            jobject row = reader.readNextRow();
            carbonRow.setCarbonRow(row);
            printf("%s\t", carbonRow.getString(1));
            printf("%d\t", carbonRow.getInt(1));
            printf("%ld\t", carbonRow.getLong(2));
            printf("%s\t", carbonRow.getVarchar(1));
            printArray(env, carbonRow.getArray(0));
            printf("%d\t", carbonRow.getShort(5));
            printf("%d\t", carbonRow.getInt(6));
            printf("%ld\t", carbonRow.getLong(7));
            printf("%lf\t", carbonRow.getDouble(8));
            printBoolean(carbonRow.getBoolean(9));
            printf("%s\t", carbonRow.getDecimal(9));
            printf("%f\t", carbonRow.getFloat(11));
            printf("\n");
            env->DeleteLocalRef(row);
        }
        reader.close();
        carbonRow.close();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        reader.close();
        carbonRow.close();
        throw e;
    }
}

/**
 * print result of reading data
 *
 * @param env JNIEnv
 * @param reader CarbonReader object
 */
void printResult(JNIEnv *env, CarbonReader reader) {
    try {
        CarbonRow carbonRow(env);
        while (reader.hasNext()) {
            jobject row = reader.readNextRow();
            carbonRow.setCarbonRow(row);
            printf("%s\t", carbonRow.getString(0));
            printf("%d\t", carbonRow.getInt(1));
            printf("%ld\t", carbonRow.getLong(2));
            //printf("%s\t", carbonRow.getVarchar(3));
            printArray(env, carbonRow.getArray(3));
            printf("%d\t", carbonRow.getShort(4));
            printf("%d\t", carbonRow.getInt(5));
            printf("%ld\t", carbonRow.getLong(6));
            printf("%lf\t", carbonRow.getDouble(7));
            printBoolean(carbonRow.getBoolean(8));
            //printf("%s\t", carbonRow.getDecimal(10));
            printf("%f\t", carbonRow.getFloat(9));
            printf("\n");
            env->DeleteLocalRef(row);
        }
        carbonRow.close();
        reader.close();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
    }
}



/**
 * test read Schema from path
 *
 * @param env jni env
 * @return whether it is success
 */
bool readSchema(JNIEnv *env, char *Path, bool validateSchema, char **argv, int argc) {
    try {
        printf("\nread Schema:\n");
        Configuration conf(env);
        if (argc > 3) {
            conf.set("fs.s3a.access.key", argv[1]);
            conf.set("fs.s3a.secret.key", argv[2]);
            conf.set("fs.s3a.endpoint", argv[3]);
        }
        printf("%s\n", conf.get("fs.s3a.endpoint", "default"));

        CarbonSchemaReader carbonSchemaReader(env);
        jobject schema;

        if (validateSchema) {
            schema = carbonSchemaReader.readSchema(Path, validateSchema, conf);
        } else {
            schema = carbonSchemaReader.readSchema(Path, conf);
        }
        Schema carbonSchema(env, schema);
        int length = carbonSchema.getFieldsLength();
        printf("schema length is:%d\n", length);
        for (int i = 0; i < length; i++) {
            printf("%d\t", i);
            printf("%s\t", carbonSchema.getFieldName(i));
            printf("%s\n", carbonSchema.getFieldDataTypeName(i));
            if (strcmp(carbonSchema.getFieldDataTypeName(i), "ARRAY") == 0) {
                printf("Array Element Type Name is:%s\n", carbonSchema.getArrayElementTypeName(i));
            }
        }
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        return false;
    }
    return true;
}



/**
 * test the exception when carbonRow with wrong index.
 *
 * @param env  jni env
 * @return
 */
bool tryCarbonRowException(JNIEnv *env, char *path) {
    printf("\nRead data from local without projection:\n");
    bool gotException= false;
    CarbonReader carbonReaderClass;
    try {
        carbonReaderClass.builder(env, path);
    } catch (runtime_error e) {
        printf("\nget exception from builder and throw\n");
        throw e;
    }
    try {
        carbonReaderClass.build();
        printResultWithException(env, carbonReaderClass);
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        gotException= true;
        carbonReaderClass.close();
    }
    return gotException;
}

/**
 * test read data from local disk, without projection
 *
 * @param env  jni env
 * @return
 */
bool readFromLocalWithoutProjection(JNIEnv *env, char *path) {
    printf("\nRead data from local without projection:\n");
    CarbonReader carbonReaderClass;
    try {
        carbonReaderClass.builder(env, path);
    } catch (runtime_error e) {
        printf("\nget exception fro builder and throw\n");
        throw e;
    }
    try {
        carbonReaderClass.build();
        printResult(env, carbonReaderClass);
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        return false;
    }
    return true;
}

/**
 * test read data by readNextRow method
 *
 * @param env  jni env
 */
bool testReadNextRow(JNIEnv *env, char *path, int printNum, char **argv, int argc, bool useVectorReader) {
    printf("\nTest next Row Performance, useVectorReader is ");
    printBoolean(useVectorReader);
    printf("\n");

    try {
        struct timeval start, build, startRead, endBatchRead, endRead;
        gettimeofday(&start, NULL);
        CarbonReader carbonReaderClass;

        carbonReaderClass.builder(env, path);
        if (argc > 1) {
            carbonReaderClass.withHadoopConf("fs.s3a.access.key", argv[1]);
            carbonReaderClass.withHadoopConf("fs.s3a.secret.key", argv[2]);
            carbonReaderClass.withHadoopConf("fs.s3a.endpoint", argv[3]);
        }
        if (!useVectorReader) {
            carbonReaderClass.withRowRecordReader();
        }
        carbonReaderClass.build();

        gettimeofday(&build, NULL);
        int time = 1000000 * (build.tv_sec - start.tv_sec) + build.tv_usec - start.tv_usec;
        double buildTime = time / 1000000.0;
        printf("\n\nbuild time is: %lf s\n\n", time / 1000000.0);

        CarbonRow carbonRow(env);
        int i = 0;

        gettimeofday(&startRead, NULL);
        jobject row;
        while (carbonReaderClass.hasNext()) {

            row = carbonReaderClass.readNextRow();

            i++;
            if (i > 1 && i % printNum == 0) {
                gettimeofday(&endBatchRead, NULL);

                time = 1000000 * (endBatchRead.tv_sec - startRead.tv_sec) + endBatchRead.tv_usec - startRead.tv_usec;
                printf("%d: time is %lf s, speed is %lf records/s  ", i, time / 1000000.0,
                       printNum / (time / 1000000.0));

                carbonRow.setCarbonRow(row);
                printf("%s\t", carbonRow.getString(0));
                printf("%s\t", carbonRow.getString(1));
                printf("%s\t", carbonRow.getString(2));
                printf("%s\t", carbonRow.getString(3));
                printf("%ld\t", carbonRow.getLong(4));
                printf("%ld\t", carbonRow.getLong(5));
                printf("\n");

                gettimeofday(&startRead, NULL);
            }
            env->DeleteLocalRef(row);
        }

        gettimeofday(&endRead, NULL);

        time = 1000000 * (endRead.tv_sec - build.tv_sec) + endRead.tv_usec - build.tv_usec;
        printf("total line is: %d,\t build time is: %lf s,\tread time is %lf s, average speed is %lf records/s  ",
               i, buildTime, time / 1000000.0, i / (time / 1000000.0));
        carbonReaderClass.close();
        carbonRow.close();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        env->ExceptionClear();
        return false;
    }
    return true;
}

/**
 * test read data by readNextBatchRow method
 *
 * @param env  jni env
 */
bool testReadNextBatchRow(JNIEnv *env, char *path, int batchSize, int printNum, char **argv, int argc,
                          bool useVectorReader) {
    bool result= true;
    try {
        printf("\n\nTest next Batch Row Performance:\n");
        printBoolean(useVectorReader);
        printf("\n");

        struct timeval start, build, read;
        gettimeofday(&start, NULL);

        CarbonReader carbonReaderClass;

        carbonReaderClass.builder(env, path);
        if (argc > 1) {
            carbonReaderClass.withHadoopConf("fs.s3a.access.key", argv[1]);
            carbonReaderClass.withHadoopConf("fs.s3a.secret.key", argv[2]);
            carbonReaderClass.withHadoopConf("fs.s3a.endpoint", argv[3]);
        }
        if (!useVectorReader) {
            carbonReaderClass.withRowRecordReader();
        }
        carbonReaderClass.withBatch(batchSize);
        carbonReaderClass.build();

        gettimeofday(&build, NULL);
        int time = 1000000 * (build.tv_sec - start.tv_sec) + build.tv_usec - start.tv_usec;
        double buildTime = time / 1000000.0;
        printf("\n\nbuild time is: %lf s\n\n", time / 1000000.0);

        CarbonRow carbonRow(env);
        int i = 0;
        struct timeval startHasNext, startReadNextBatchRow, endReadNextBatchRow, endRead;
        gettimeofday(&startHasNext, NULL);

        while (carbonReaderClass.hasNext()) {

            gettimeofday(&startReadNextBatchRow, NULL);
            jobjectArray batch = carbonReaderClass.readNextBatchRow();
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
            }
            gettimeofday(&endReadNextBatchRow, NULL);

            jsize length = env->GetArrayLength(batch);
            if (i + length > printNum - 1) {
                for (int j = 0; j < length; j++) {
                    i++;
                    jobject row = env->GetObjectArrayElement(batch, j);
                    carbonRow.setCarbonRow(row);
                    carbonRow.getString(0);
                    carbonRow.getString(1);
                    carbonRow.getString(2);
                    carbonRow.getString(3);
                    carbonRow.getLong(4);
                    carbonRow.getLong(5);
                    if (i > 1 && i % printNum == 0) {
                        gettimeofday(&read, NULL);

                        double hasNextTime = 1000000 * (startReadNextBatchRow.tv_sec - startHasNext.tv_sec) +
                                             startReadNextBatchRow.tv_usec - startHasNext.tv_usec;

                        double readNextBatchTime = 1000000 * (endReadNextBatchRow.tv_sec - startReadNextBatchRow.tv_sec) +
                                                   endReadNextBatchRow.tv_usec - startReadNextBatchRow.tv_usec;

                        time = 1000000 * (read.tv_sec - startHasNext.tv_sec) + read.tv_usec - startHasNext.tv_usec;
                        printf("%d: time is %lf s, speed is %lf records/s, hasNext time is %lf s,readNextBatchRow time is %lf s ",
                               i, time / 1000000.0, printNum / (time / 1000000.0), hasNextTime / 1000000.0,
                               readNextBatchTime / 1000000.0);
                        gettimeofday(&startHasNext, NULL);
                        printf("%s\t", carbonRow.getString(0));
                        printf("%s\t", carbonRow.getString(1));
                        printf("%s\t", carbonRow.getString(2));
                        printf("%s\t", carbonRow.getString(3));
                        printf("%ld\t", carbonRow.getLong(4));
                        printf("%ld\t", carbonRow.getLong(5));
                        printf("\n");
                    }
                    env->DeleteLocalRef(row);
                }
            } else {
                i = i + length;
            }
            env->DeleteLocalRef(batch);
        }
        gettimeofday(&endRead, NULL);
        time = 1000000 * (endRead.tv_sec - build.tv_sec) + endRead.tv_usec - build.tv_usec;
        printf("total line is: %d,\t build time is: %lf s,\tread time is %lf s, average speed is %lf records/s  ",
               i, buildTime, time / 1000000.0, i / (time / 1000000.0));
        carbonReaderClass.close();
        carbonRow.close();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        env->ExceptionClear();
        result= false;
    }
    return result;
}

/**
 * test read data from local disk
 *
 * @param env  jni env
 * @return
 */
bool readFromLocalWithProjection(JNIEnv *env, char *path) {
    printf("\nRead data from local:\n");
    try {
        CarbonReader reader;
        reader.builder(env, path, "test");

        char *argv[10];
        argv[0] = "stringField";
        argv[1] = "shortField";
        argv[2] = "intField";
        argv[3] = "longField";
        argv[4] = "doubleField";
        argv[5] = "boolField";
        argv[6] = "dateField";
        argv[7] = "timeField";
        //argv[8] = "decimalField";
        //argv[9] = "varcharField";
        argv[8] = "arrayField";
        argv[9] = "floatField";
        reader.projection(10, argv);

        reader.build();

        CarbonRow carbonRow(env);
        while (reader.hasNext()) {
            jobject row = reader.readNextRow();
            carbonRow.setCarbonRow(row);

            printf("%s\t", carbonRow.getString(0));
            printf("%d\t", carbonRow.getShort(1));
            printf("%d\t", carbonRow.getInt(2));
            printf("%ld\t", carbonRow.getLong(3));
            printf("%lf\t", carbonRow.getDouble(4));
            printBoolean(carbonRow.getBoolean(5));
            printf("%d\t", carbonRow.getInt(6));
            printf("%ld\t", carbonRow.getLong(7));
            //printf("%s\t", carbonRow.getDecimal(8));
           //printf("%s\t", carbonRow.getVarchar(9));
            printArray(env, carbonRow.getArray(8));
            printf("%f\t", carbonRow.getFloat(9));
            printf("\n");
            env->DeleteLocalRef(row);
        }

        reader.close();
        carbonRow.close();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        env->ExceptionClear();
        printf("\n Reade Data from local is failed :\n");
        return false;
    }
    return true;
}


bool tryCatchException(JNIEnv *env) {
    printf("\ntry catch exception and print:\n");
    bool gotException= false;
    CarbonReader carbonReaderClass;
    carbonReaderClass.builder(env, "./carbondata");
    try {
        carbonReaderClass.build();
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        env->ExceptionClear();
        gotException= true;
    }
    printf("\nfinished handle exception\n");
    return gotException;
}

bool testCarbonProperties(JNIEnv *env) {
    try {
        printf("%s", "test Carbon Properties:");
        CarbonProperties carbonProperties(env);
        char *key = "carbon.unsafe.working.memory.in.mb";
        printf("%s\t", carbonProperties.getProperty(key));
        printf("%s\t", carbonProperties.getProperty(key, "512"));
        carbonProperties.addProperty(key, "1024");
        char *current = carbonProperties.getProperty(key);
        printf("%s\t", current);
        if (strcmp(current, "1024") == 0) {
            return true;
        } else {
            return false;
        }
    } catch (jthrowable e) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(e));
        env->ExceptionClear();
        return false;
    }
}

/**
 * test write data
 * test WithLoadOption interface
 *
 * @param env  jni env
 * @param path file path
 * @param argc argument counter
 * @param argv argument vector
 * @return true or throw exception
 */
bool testWriteData(JNIEnv *env, char *path, int argc, char *argv[],int rowNum) {

    char *jsonSchema = "[{stringField:string},{shortField:short},{intField:int},{longField:long},{doubleField:double},{boolField:boolean},{dateField:date},{timeField:timestamp},{floatField:float},{arrayField:array}]";
    CarbonWriter writer;
    try {
        writer.builder(env);
        writer.outputPath(path);
        writer.withCsvInput(jsonSchema);
        writer.withLoadOption("complex_delimiter_level_1", "#");
        writer.writtenBy("CSDK");
        writer.taskNo(185);
        writer.withThreadSafe(1);
        writer.uniqueIdentifier(1549911814000000);
        writer.withBlockSize(1);
        writer.withBlockletSize(16);
        writer.enableLocalDictionary(true);
        writer.localDictionaryThreshold(10000);
        if (argc > 3) {
            writer.withHadoopConf("fs.s3a.access.key", argv[1]);
            writer.withHadoopConf("fs.s3a.secret.key", argv[2]);
            writer.withHadoopConf("fs.s3a.endpoint", argv[3]);
        }
        writer.build();

        int size = 10;
        long longValue = 0;
        double doubleValue = 0;
        float floatValue = 0;
        jclass objClass = env->FindClass("java/lang/String");
        for (int i = 0; i < rowNum; ++i) {
            jobjectArray arr = env->NewObjectArray(size, objClass, 0);
            char ctrInt[10];
            gcvt(i, 10, ctrInt);

            char a[15] = "robot";
            strcat(a, ctrInt);
            jobject stringField = env->NewStringUTF(a);
            env->SetObjectArrayElement(arr, 0, stringField);

            char ctrShort[10];
            gcvt(i % 10000, 10, ctrShort);
            jobject shortField = env->NewStringUTF(ctrShort);
            env->SetObjectArrayElement(arr, 1, shortField);

            jobject intField = env->NewStringUTF(ctrInt);
            env->SetObjectArrayElement(arr, 2, intField);


            char ctrLong[10];
            gcvt(longValue, 10, ctrLong);
            longValue = longValue + 2;
            jobject longField = env->NewStringUTF(ctrLong);
            env->SetObjectArrayElement(arr, 3, longField);

            char ctrDouble[10];
            gcvt(doubleValue, 10, ctrDouble);
            doubleValue = doubleValue + 2;
            jobject doubleField = env->NewStringUTF(ctrDouble);
            env->SetObjectArrayElement(arr, 4, doubleField);

            jobject boolField = env->NewStringUTF("true");
            env->SetObjectArrayElement(arr, 5, boolField);

            jobject dateField = env->NewStringUTF(" 2019-03-02");
            env->SetObjectArrayElement(arr, 6, dateField);

            jobject timeField = env->NewStringUTF("2019-02-12 03:03:34");
            env->SetObjectArrayElement(arr, 7, timeField);

            char ctrFloat[10];
            gcvt(floatValue, 10, ctrFloat);
            floatValue = floatValue + 2;
            jobject floatField = env->NewStringUTF(ctrFloat);
            env->SetObjectArrayElement(arr, 8, floatField);

            jobject arrayField = env->NewStringUTF("Hello#World#From#Carbon");
            env->SetObjectArrayElement(arr, 9, arrayField);


            writer.write(arr);

            env->DeleteLocalRef(stringField);
            env->DeleteLocalRef(shortField);
            env->DeleteLocalRef(intField);
            env->DeleteLocalRef(longField);
            env->DeleteLocalRef(doubleField);
            env->DeleteLocalRef(floatField);
            env->DeleteLocalRef(dateField);
            env->DeleteLocalRef(timeField);
            env->DeleteLocalRef(boolField);
            env->DeleteLocalRef(arrayField);
            env->DeleteLocalRef(arr);
        }
        writer.close();
        } catch (jthrowable ex) {
                env->ExceptionDescribe();
                printf("%s",convertExceptionToString(ex));
                env->ExceptionClear();
                writer.close();
                // If load fail no need to go for read.
                return false;
            }
    CarbonReader carbonReader;
    CarbonRow carbonRow(env);
    try{
        carbonReader.builder(env, path);
        carbonReader.build();
        int i = 0;
        int printNum = 10;
        while (carbonReader.hasNext()) {
            jobject row = carbonReader.readNextRow();
            i++;
            carbonRow.setCarbonRow(row);
            if (i < printNum) {
                printf("%s\t%d\t%ld\t", carbonRow.getString(0), carbonRow.getInt(1), carbonRow.getLong(2));
                jobjectArray array1 = carbonRow.getArray(3);
                jsize length = env->GetArrayLength(array1);
                int j = 0;
                for (j = 0; j < length; j++) {
                    jobject element = env->GetObjectArrayElement(array1, j);
                    char *str = (char *) env->GetStringUTFChars((jstring) element, JNI_FALSE);
                    printf("%s\t", str);
                }
                printf("%d\t", carbonRow.getShort(4));
                printf("%d\t", carbonRow.getInt(5));
                printf("%ld\t", carbonRow.getLong(6));
                printf("%lf\t", carbonRow.getDouble(7));
                bool bool1 = carbonRow.getBoolean(8);
                if (bool1) {
                    printf("true\t");
                } else {
                    printf("false\t");
                }
                printf("%f\t\n", carbonRow.getFloat(9));
            }
            env->DeleteLocalRef(row);
        }
        carbonRow.close();
        carbonReader.close();
    } catch (jthrowable ex) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(ex));
        env->ExceptionClear();
        carbonRow.close();
        carbonReader.close();
        return false;
    }
    return true;
}

void writeData(JNIEnv *env, CarbonWriter writer, int size, jclass objClass, char *stringField, short shortField) {
    jobjectArray arr = env->NewObjectArray(size, objClass, 0);

    jobject jStringField = env->NewStringUTF(stringField);
    env->SetObjectArrayElement(arr, 0, jStringField);

    char ctrShort[10];
    gcvt(shortField % 10000, 10, ctrShort);
    jobject jShortField = env->NewStringUTF(ctrShort);
    env->SetObjectArrayElement(arr, 1, jShortField);

    writer.write(arr);

    env->DeleteLocalRef(jStringField);
    env->DeleteLocalRef(jShortField);
    env->DeleteLocalRef(arr);
}

/**
  * test WithTableProperties interface
  *
  * @param env  jni env
  * @param path file path
  * @param argc argument counter
  * @param argv argument vector
  * @return true or throw exception
  */
bool testWithTableProperty(JNIEnv *env, char *path, int argc, char **argv) {

    char *jsonSchema = "[{stringField:string},{shortField:short}]";
    CarbonWriter writer;
    CarbonReader carbonReader;
    try {
        writer.builder(env);
        writer.outputPath(path);
        writer.withCsvInput(jsonSchema);
        writer.withTableProperty("sort_columns", "shortField");
        writer.writtenBy("CSDK");
        if (argc > 3) {
            writer.withHadoopConf("fs.s3a.access.key", argv[1]);
            writer.withHadoopConf("fs.s3a.secret.key", argv[2]);
            writer.withHadoopConf("fs.s3a.endpoint", argv[3]);
        }
        writer.build();

        int size = 10;
        jclass objClass = env->FindClass("java/lang/String");

        writeData(env, writer, size, objClass, "name3", 22);
        writeData(env, writer, size, objClass, "name1", 11);
        writeData(env, writer, size, objClass, "name2", 33);
        writer.close();


        carbonReader.builder(env, path);
        carbonReader.build();
        int i = 0;
        CarbonRow carbonRow(env);
        while (carbonReader.hasNext()) {
            jobject row = carbonReader.readNextRow();
            i++;
            carbonRow.setCarbonRow(row);
            printf("%d\t%s\t\n", carbonRow.getShort(0), carbonRow.getString(1));
            env->DeleteLocalRef(row);
        }
        carbonReader.close();
    } catch (jthrowable ex) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(ex));
        env->ExceptionClear();
        writer.close();
        carbonReader.close();
        return false;
    }
    return true;
}

/**
  * test sortBy interface
  *
  * @param env  jni env
  * @param path file path
  * @param argc argument counter
  * @param argv argument vector
  * @return true or throw exception
  */
bool testSortBy(JNIEnv *env, char *path, int argc, char **argv) {

    char *jsonSchema = "[{stringField:string},{shortField:short}]";
    char *sort[1];
    sort[0] = "shortField";
    CarbonWriter writer;
    CarbonReader carbonReader;
    try {
        writer.builder(env);
        writer.outputPath(path);
        writer.withCsvInput(jsonSchema);
        writer.sortBy(1, sort);
        writer.writtenBy("CSDK");
        if (argc > 3) {
            writer.withHadoopConf("fs.s3a.access.key", argv[1]);
            writer.withHadoopConf("fs.s3a.secret.key", argv[2]);
            writer.withHadoopConf("fs.s3a.endpoint", argv[3]);
        }
        writer.build();
        int size = 10;
        jclass objClass = env->FindClass("java/lang/String");

        writeData(env, writer, size, objClass, "name3", 22);
        writeData(env, writer, size, objClass, "name1", 11);
        writeData(env, writer, size, objClass, "name2", 33);
        writer.close();


        carbonReader.builder(env, path);
        carbonReader.build();
        int i = 0;
        CarbonRow carbonRow(env);
        while (carbonReader.hasNext()) {
            jobject row = carbonReader.readNextRow();
            i++;
            carbonRow.setCarbonRow(row);
            printf("%d\t%s\t\n", carbonRow.getShort(0), carbonRow.getString(1));
            env->DeleteLocalRef(row);
        }
        carbonReader.close();
    } catch (jthrowable ex) {
        env->ExceptionDescribe();
        printf("%s",convertExceptionToString(ex));
        env->ExceptionClear();
        writer.close();
        carbonReader.close();
        return false;
    }
    return true;
}

/**
 * read data from S3
 * parameter is ak sk endpoint
 *
 * @param env jni env
 * @param argv argument vector
 * @return
 */
bool readFromS3(JNIEnv *env, char *path, char *argv[]) {
    printf("\nRead data from S3:\n");
    CarbonReader reader;

    reader.builder(env, path, "test");
    reader.withHadoopConf("fs.s3a.access.key", argv[1]);
    reader.withHadoopConf("fs.s3a.secret.key", argv[2]);
    reader.withHadoopConf("fs.s3a.endpoint", argv[3]);
    reader.build();
    printResult(env, reader);
}



TEST(CSDKTest,tryCatchException) {
    try{
        bool gotExp=tryCatchException(env);
        EXPECT_TRUE(gotExp);
    } catch (runtime_error e){
        EXPECT_TRUE(true);
    }
}


TEST(CSDKTest,tryCarbonRowException) {
    char *smallFilePath = "../../../../resources/carbondata";
    try {
        bool result = tryCarbonRowException(env, smallFilePath);;
        EXPECT_TRUE(result) << "Expected Exception as No Index File" << result;
    } catch (runtime_error e) {
        EXPECT_TRUE(true);
    }
}

TEST(CSDKTest,testCarbonProperties) {
    try {
        bool result = testCarbonProperties(env);
        EXPECT_TRUE(result) << "Carbon set properties not working ";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while setting carbon properties";
    }
}

TEST(CSDKTest,testWriteData) {
    try {
        bool result =testWriteData(env, "./data", my_argc, my_argv,10);
        //incremental load
        result = result && testWriteData(env, "./data", my_argc, my_argv,10);
        EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while data loading";
    }
}

TEST(CSDKTest,readFromLocalWithoutProjection) {
    try {
        char *smallFilePath = "./data_withoutpro";
        bool result =testWriteData(env, smallFilePath, my_argc, my_argv,10);
        if(result){
            bool proj_result = readFromLocalWithoutProjection(env, smallFilePath);
            EXPECT_TRUE(proj_result) << "Without Projection is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During without projection selection";
    }
}

TEST(CSDKTest,readFromLocalWithProjection) {
    try {
        char *smallFilePath = "./data_pro";
        bool result =testWriteData(env, smallFilePath, my_argc, my_argv,10);
        if(result){
            bool proj_result = readFromLocalWithProjection(env, smallFilePath);
            EXPECT_TRUE(proj_result) << "With Projection is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During With projection selection";
    }
}


TEST(CSDKTest,readSchemaWithoutValidation) {
    try {
        char *path = "./data_readPath";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool schema_result = readSchema(env, path, false,my_argv, 1);
            EXPECT_TRUE(schema_result) << "Not Able to read readSchema from given path";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Read Schema";
    }
}


TEST(CSDKTest,readSchemaWithValidation) {
    try {
        char *path = "./data_readPathWithValidation";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool schema_result = readSchema(env, path, true,my_argv, 1);
            EXPECT_TRUE(schema_result) << "Not Able to read readSchema Or validate from given path";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Read Schema";
    }
}

TEST(CSDKTest,testWithTableProperty) {
    try {
        char *path = "./tableProperty";
        bool result = testWithTableProperty(env, path, 1, my_argv);
        EXPECT_TRUE(result) << "Data Loading with Table properties is failled.";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Read Schema";
    }
}

TEST(CSDKTest,testSortBy) {
    try {
        char *path = "./testSortBy";
        bool result = testSortBy(env, path, 1, my_argv);
        EXPECT_TRUE(result) << "data & read sorted data is failed";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Read Schema";
    }
}

TEST(CSDKTest,testReadNextRowWthtVector) {
    try {
        int printNum = 32000;
        char *path = "./data_forVector";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool readresultWithVector= testReadNextRow(env, path, printNum, my_argv, 0, true);
            EXPECT_TRUE(readresultWithVector) << "Vector reading is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Vector read";
    }
}


TEST(CSDKTest,testReadNextRowWthoutVector) {
    try {
        int printNum = 32000;
        char *path = "./data_forCarbonReader";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool readresultWithVectorfalse=testReadNextRow(env, path, printNum, my_argv, 0, false);
            EXPECT_TRUE(readresultWithVectorfalse) << "Carbon Reader (Vector=false) is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Carbon Read";
    }
}


TEST(CSDKTest,testReadNextBatchRowWithVector) {
    try {
        int printNum = 32000;
        int batch = 32000;
        char *path = "./data_forCarbonReader";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool readresultWithVector=testReadNextBatchRow(env, path, batch, printNum, my_argv, 0, true);
            EXPECT_TRUE(readresultWithVector) << "Vector Reader With Batch  is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Vector with Batch Read";
    }
}

TEST(CSDKTest,testReadNextBatchRowWithoutVector) {
    try {
        int printNum = 32000;
        int batch = 32000;
        char *path = "./data_forCarbonReader";
        //char *S3WritePath = "s3a://csdk/WriterOutput/carbondata2";
        bool result =testWriteData(env, path, my_argc, my_argv,10);
        if(result){
            bool readresultWithoutVector=testReadNextBatchRow(env, path, batch, printNum, my_argv, 0, false);
            EXPECT_TRUE(readresultWithoutVector) << "Carbon Reader with batch(Vector=false) is failed";
        } else {
            EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed";
        }
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected ,During Carbon Read with batch";
    }
}

/**
 * S3 releated Testcases are Ignore/DISABLED in CI
 */
TEST(CSDKTest,DISABLED_testWriteDataS3) {
    char *S3WritePath = "s3a://csdk/WriterOutput/carbondata2";
    try {
        bool result =testWriteData(env, S3WritePath, 4, my_argv,10);
        EXPECT_TRUE(result) << "Either Data Loading Or Carbon Reader  is failed in S3";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while data loading  in S3";
    }
}

TEST(CSDKTest,DISABLED_readFromS3) {
    char *S3ReadPath = "s3a://csdk/WriterOutput/carbondata";
    try {
        bool result =readFromS3(env, S3ReadPath,my_argv);
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while reading in S3";
    }
}

TEST(CSDKTest, DISABLED_readSchemaS3) {
    char *S3WritePath = "s3a://csdk/WriterOutput/carbondata";
    try {
        bool result = readSchema(env, S3WritePath, true, my_argv, 4);
        result = result && readSchema(env, S3WritePath, false, my_argv, 4);
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while reading Schema in S3";
    }
}


TEST(CSDKTest,DISABLED_testReadNextRowFromS3) {
    char *S3Path = "s3a://csdk/bigData/i400bs128";
    try {
        bool result=testReadNextRow(env, S3Path, 100000, my_argv, 4, false);
        result=result&&testReadNextRow(env, S3Path, 100000, my_argv, 4, true);
        result=result&&testReadNextBatchRow(env, S3Path, 100000, 100000, my_argv, 4, false);
        result=result&&testReadNextBatchRow(env, S3Path, 100000, 100000, my_argv, 4, true);
        EXPECT_TRUE(result) << "Read from S3 is failed with vactor and batch";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while reading in S3";
    }
}

TEST(CSDKTest,DISABLED_testSortByS3) {
    char *S3Path = "s3a://csdk/bigData/dataSort";
    try {
        bool result=testSortBy(env, "s3a://csdk/dataSort", 4, my_argv);
        EXPECT_TRUE(result) << "Read from S3 is failed with sort";
    } catch (runtime_error e) {
        EXPECT_TRUE(false) << " Exception is not expected while reading in S3";
    }
}


/**
 * This a example for C++ interface to read carbon file
 * If you want to test read data fromS3, please input the parameter: ak sk endpoint
 *
 * @param argc argument counter
 * @param argv argument vector
 * @return
 */
int main(int argc, char *argv[]) {
    // init jvm
    env = initJVM();
    my_argc = argc;
    my_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
    (jvm)->DestroyJavaVM();

    cout << "\nfinish destroy jvm";
    return 0;
}

