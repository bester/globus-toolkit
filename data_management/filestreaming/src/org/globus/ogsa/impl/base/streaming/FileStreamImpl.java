package org.globus.ogsa.impl.base.streaming;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.rmi.RemoteException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Vector;

import javax.xml.namespace.QName;

import org.apache.axis.MessageContext;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.log4j.Logger;

import org.globus.axis.gsi.GSIConstants;
import org.globus.gatekeeper.jobmanager.internal.Tail;
import org.globus.gsi.gssapi.auth.SelfAuthorization;
import org.globus.io.streams.FTPOutputStream;
import org.globus.io.streams.GassOutputStream;
import org.globus.io.streams.GlobusFileOutputStream;
import org.globus.io.streams.GridFTPOutputStream;
import org.globus.io.streams.HTTPOutputStream;
import org.globus.ogsa.base.streaming.FileStreamAttributes;
import org.globus.ogsa.base.streaming.FileStreamFactoryAttributes;
import org.globus.ogsa.base.streaming.FileStreamPortType;
import org.globus.ogsa.GridConstants;
import org.globus.ogsa.GridContext;
import org.globus.ogsa.GridServiceException;
import org.globus.ogsa.impl.ogsi.GridServiceImpl;
import org.globus.ogsa.impl.security.authentication.SecureServicePropertiesHelper;
import org.globus.ogsa.impl.security.authentication.SecurityManager;
import org.globus.ogsa.impl.security.authentication.Constants;
import org.globus.ogsa.ServiceProperties;
import org.globus.ogsa.repository.ServiceNode;
import org.globus.ogsa.ServiceData;
import org.globus.util.GlobusURL;

import org.ietf.jgss.GSSCredential;

public class FileStreamImpl extends GridServiceImpl {
    
    static Log logger = LogFactory.getLog (FileStreamImpl.class.getName());

    private static final String DEST_URL_SDE_NAME = "destinationUrl";
    protected Tail outputFollower;
    protected boolean appendStdout = true;
    protected GSSCredential proxy = null;
    private String sourcePath;
    private String destinationUrl;
    private int offset;
    private OutputStream outputStream;
    private Vector fileStreamStateListeners = new Vector();

    public FileStreamImpl(FileStreamFactoryAttributes factoryAttributes,
                          FileStreamAttributes streamAttributes) {
        super("FileStreamImpl");

        //equivalent of SecureRPCURIProvider.setExcludedMethods(this, "");

        String name = "FileStream";
        String id = String.valueOf(hashCode());
        if(id != null) {
            name = name + "(" + id + ")";
        }

        setProperty (ServiceProperties.NAME, name);

        this.sourcePath = factoryAttributes.getSourcePath();
        this.offset = streamAttributes.getOffset();
        this.destinationUrl = streamAttributes.getDestinationUrl();
    }

    private void addDestinationUrlServiceData() throws GridServiceException {
        ServiceData destinationUrlServiceData =
            this.serviceData.create(DEST_URL_SDE_NAME);
        destinationUrlServiceData.setValue(this.destinationUrl);
        this.serviceData.add(destinationUrlServiceData);
    }
    
    public void postCreate(GridContext context) throws GridServiceException {
        SecurityManager manager = SecurityManager.getManager();
        manager.setServiceOwnerFromContext(this, context);
    }
    
    protected OutputStream openUrl(String file) throws RemoteException {
        GlobusURL url = null;
        try {
            url = new GlobusURL(file);
        } catch(Exception e) {
            throw new RemoteException("Invalid URL");
        }
        try {
            return openUrl(url);
        } catch(Exception e) {
            logger.debug("Failed to open remote URL", e);
            throw new RemoteException("Failed to open remote URL", e);
        }
    }

    protected OutputStream openUrl(GlobusURL url) throws Exception {
        String protocol = url.getProtocol();
        if (protocol.equalsIgnoreCase("https")) {
            return new GassOutputStream(this.proxy,
                                        url.getHost(),
                                        url.getPort(),
                                        url.getPath(),
                                        -1,
                                        appendStdout);
        } else if (protocol.equalsIgnoreCase("http")) {
            return new HTTPOutputStream(url.getHost(),
                                        url.getPort(),
                                        url.getPath(),
                                        -1,
                                        appendStdout);
        } else if (protocol.equalsIgnoreCase("gsiftp")) {
            return new GridFTPOutputStream(this.proxy, 
                                          url.getHost(),
                                          url.getPort(),
                                          url.getPath(),
                                          appendStdout);
        } else if (protocol.equalsIgnoreCase("ftp")) {
            return new FTPOutputStream(url.getHost(),
                                       url.getPort(),
                                       url.getUser(),
                                       url.getPwd(),
                                       url.getPath(),
                                       appendStdout);
        } else if (protocol.equalsIgnoreCase("file")) {
            return new GlobusFileOutputStream(url.getPath(), appendStdout);
        } else {
            throw new Exception("Protocol not supported: " + protocol);
        }
    }

    public void preDestroy() {
        try {
            outputStream.close();
            logger.debug("File Stream instance is destroyed");
        }catch(java.io.IOException ioe) {
            logger.error("Error in destroying the File Stream Instance",ioe);
        }
    }

    public void addFileStreamStateListener(
            FileStreamStateListener                 listener) {
        this.fileStreamStateListeners.add(listener);
    }

    public void removeFileStreamStateListener(
            FileStreamStateListener                 listener) {
        this.fileStreamStateListeners.remove(listener);
    }

    public void fireFileStreamStarted() {
        Iterator listenerIter = this.fileStreamStateListeners.iterator();
        while (listenerIter.hasNext()) {
            FileStreamStateListener listener
                = (FileStreamStateListener) listenerIter.next();
            listener.fileStreamStarted();
        }
    }

    public void fireFileStreamStopped() {
        Iterator listenerIter = this.fileStreamStateListeners.iterator();
        while (listenerIter.hasNext()) {
            FileStreamStateListener listener
                = (FileStreamStateListener) listenerIter.next();
            listener.fileStreamStopped();
        }
    }

    private void assertSecurity() throws RemoteException {
        MessageContext messageContext = MessageContext.getCurrentContext();
        if (messageContext.getProperty(Constants.CONTEXT) == null) {
            throw new RemoteException("service requires secure access");
        }
    }

    public void start() throws RemoteException {
        assertSecurity();

        GSSCredential credential
            = SecureServicePropertiesHelper.getCredential(this);
        File outputFile = new File(this.sourcePath);
        this.proxy = credential;
        outputStream = openUrl(destinationUrl);

        if(this.outputFollower == null) {
            this.outputFollower = new Tail();
            this.outputFollower.setLogger(
                Logger.getLogger(FileStreamImpl.class.getName()));
            this.outputFollower.start();
        }
        try { 
            this.outputFollower.addFile(outputFile,outputStream,offset);
        } catch(IOException e) {
            logger.error("Error stream file", e);
            throw new RemoteException("Error in Stream");
        }

        fireFileStreamStarted();
    }

    public void stop() throws RemoteException {
        assertSecurity();

        logger.debug("stopping stream");
        try {
            addDestinationUrlServiceData();
            this.outputFollower.stop();
            boolean joined = false;

            while (!joined) {
                try {
                    this.outputFollower.join();
                    joined = true;
                    if (outputStream != null) {
                        outputStream.close();
                    }
                } catch (InterruptedException ie) {
                } catch (IOException ioe) {
                }
            }

            fireFileStreamStopped();
        } catch (GridServiceException gse) {
            logger.error("problem adding service data", gse);
        }
    }
}
