package org.globus.ogsa.base.gram.testing.scalability;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

public class ScalabilityTester {

    static Log logger = LogFactory.getLog(ScalabilityTester.class.getName());

    String factoryUrl = null;
    String rslFile = null;
    int count = 1;
    //JobStarterThread[] jobList = null;
    String[] jobHandleList = null;
    int[] jobPhaseState = null;
    boolean[] startedList = null;
    int startedCount = 0;

    public ScalabilityTester() { }

    public synchronized void run() {
        createAll();

        waitForAllToComplete();
    }

    protected synchronized void createAll() {
        //this.jobList = new JobStarterThread[this.count];
        this.jobHandleList = new String[this.count];
        this.jobPhaseState = new int[this.count];
        this.startedList = new boolean[this.count];
        for (int index=0; index<this.count; index++) {
            this.startedList[index] = false;
        }

        if (logger.isDebugEnabled()) {
            logger.debug("creating " + this.count + " job(s)");
        }

        for (int i=0; i<this.count; i++) {
            //this.jobList[i] = new JobStarterThread(this, i);
            //new Thread(this.jobList[i]).start();
            new Thread(new JobStarterThread(this, i)).start();
            //if ((i > 0) && (i % 20) == 0) {
                try {
                    wait(1000);
                } catch (Exception e) {
                    logger.error("error waiting for next submission", e);
                }
            //}
        }

        if (logger.isDebugEnabled()) {
            logger.debug("all jobs created");
        }
    }

    protected void waitForAllToComplete() {
        int oldCompletedCount = -1;
        while (this.startedCount < this.count) {
            if (logger.isDebugEnabled()) {
                if (oldCompletedCount != this.startedCount) {
                    logger.debug("waiting for "
                                + (this.count - this.startedCount)
                                + " job(s) to be started");
                    oldCompletedCount = this.startedCount;
                }
            }

            try {
                wait(5000);
            } catch (Exception e) {
                logger.error("unabled to wait", e);
            }
            this.startedCount = 0;
            for (int index=0; index<this.count; index++) {
                if (this.startedList[index]) {
                    this.startedCount++;
                } else {
                    if (logger.isDebugEnabled()) {
                        logger.debug("Waiting for job #" + index);
                    }
                }
            }
        }

        if (logger.isDebugEnabled()) {
            logger.debug("all jobs started");
        }
    }

    synchronized void notifyError() {
        this.count--;
        notifyAll();
    }

    synchronized void notifyCreated(int jobIndex, String jobHandle) {
        if (logger.isDebugEnabled()) {
            logger.debug("got created signal from job #" + jobIndex);
        }
        this.jobHandleList[jobIndex] = jobHandle;
    }

    synchronized void notifyStarted(int jobIndex) {
        if (logger.isDebugEnabled()) {
            logger.debug("got started signal from job #" + jobIndex);
        }
        this.startedList[jobIndex] = true;
        notifyAll();
    }

    public void setFactoryUrl(String factoryUrl) {
        this.factoryUrl = factoryUrl;
    }

    public String getFactoryUrl() {
        return this.factoryUrl;
    }

    public void setRslFile(String rslFile) {
        this.rslFile = rslFile;
    }

    public String getRslFile() {
        return this.rslFile;
    }

    public void setCount(int count) {
        this.count = count;
    }

    public int getCount() {
        return this.count;
    }

    public static void printUsage(String customMessage) {
        StringBuffer usageMessage = new StringBuffer(customMessage);
        usageMessage.append("\nUsage:");
        usageMessage.append("\njava ... ");
        usageMessage.append(ScalabilityTester.class.getName()).append(" \\");
        usageMessage.append("\n\t<factory URL> <RSL file> <job count>");
        System.out.println(usageMessage.toString());
    }

    public static void main(String[] args) {
        if (args.length == 1) {
            if (   args[0].equals("-h")
                || args[0].equals("--help")) {
                ScalabilityTester.printUsage("-- Help --");
                System.exit(0);
            }

        }
        if (args.length != 3) {
            ScalabilityTester.printUsage("Error: invalid number of arguments");
            System.exit(1);
        }
        if (logger.isDebugEnabled()) {
            logger.debug("Factory URL: " + args[0]);
            logger.debug("RSL File: " + args[1]);
            logger.debug("Job Count: " + args[2]);
        }

        ScalabilityTester harness = new ScalabilityTester();
        harness.setFactoryUrl(args[0]);
        harness.setRslFile(args[1]);
        harness.setCount(Integer.parseInt(args[2]));

        harness.run();
    }
}
