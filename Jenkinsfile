#!/usr/bin/env groovy

pipeline {

    agent {
        docker {
            image 'lsstts/rotator_pxi:v0.3'
        }
    }

    triggers {
        pollSCM('H * * * *')
    }

    environment {
        // XML report path
        XML_REPORT = "jenkinsReport/report.xml"
        XML_REPORT_COVERAGE = "jenkinsReportCov/reportCoverage.xml"
        // Module path
        PXI_CNTLR_HOME = "$WORKSPACE"
    }

    stages {

        stage('Build Executable') {
            steps {
                // 'PATH' can only be updated in a single shell block.
                // We can not update PATH in 'environment' block.
                withEnv(["HOME=${env.WORKSPACE}"]) {
                    sh """
                        cd tests/
                        make
                    """
                }
            }
        }

        stage('Unit Tests and Code Coverage') {
            steps {
                // 'PATH' can only be updated in a single shell block.
                // We can not update PATH in 'environment' block.
                withEnv(["HOME=${env.WORKSPACE}"]) {
                    sh """
                        ./bin/test --gtest_output=xml:${env.XML_REPORT}

                        mkdir jenkinsReportCov/

                        cd build/tests/
                        gcovr -r ../../src/ . --xml-pretty > ${HOME}/${env.XML_REPORT_COVERAGE}
                    """
                }
            }
        }
    }

    post {
        always {
            // The path of xml needed by xUnit is relative to
            // the workspace.
            xunit (
                thresholds: [ skipped(failureThreshold: '0'), failed(failureThreshold: '0') ],
                tools: [ GoogleTest(pattern: 'jenkinsReport/*.xml') ]
            )

            // Publish the coverage report
            cobertura coberturaReportFile: 'jenkinsReportCov/*.xml'
        }

        cleanup {
            // clean up the workspace
            deleteDir()
        }
    }
}
