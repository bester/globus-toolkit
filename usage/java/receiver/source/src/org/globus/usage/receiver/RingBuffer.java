package org.globus.usage.receiver;

import org.globus.usage.packets.CustomByteBuffer;

/*I don't think this class exists yet in the java platform.
  It's pretty trivial to implement one.
Since one thread will be writing into this while another thread reads
out, it must be thread-safe.*/

public class RingBuffer {

    private CustomByteBuffer[] queue;
    private int numObjects, maxObjects;
    private int inputIndex, outputIndex;


    public RingBuffer(int capacity) {
        maxObjects = capacity;
        numObjects = 0;
        queue = new CustomByteBuffer[maxObjects];
        inputIndex = outputIndex = 0;
    }

    /*Returns and removes next object (FIFO) if there is one;
      if ringbuffer is empty, returns null.*/
    public synchronized CustomByteBuffer getNext() {
        if (numObjects == 0)
            return null;
        else {
            CustomByteBuffer theNext;
            theNext = queue[outputIndex];
            queue[outputIndex] = null;
            outputIndex = (outputIndex + 1) % maxObjects;
            numObjects --;
            return theNext;
        }
    }

    /*Returns true if insert was successful, false if ringbuffer 
      was already full and the insert failed.*/
    public synchronized boolean insert(CustomByteBuffer newBuf) {
        if (numObjects == maxObjects)
            return false;
        else {
            queue[inputIndex] = newBuf;
            inputIndex = (inputIndex + 1) % maxObjects;
            numObjects ++;
            return true;
        }
    }

    /*These query methods are synchronized so that they can't be called when
      the other thread is halfway through inserting or removing, which might
      give the wrong answer.*/
    public synchronized boolean isFull() {
        return numObjects == maxObjects;
    }

    public synchronized boolean isEmpty() {
        return numObjects == 0;
    }

    public synchronized int getCapacity() {
        return maxObjects;
    }
    
    public synchronized int getNumObjects() {
        return numObjects;
    }

    /*JUnit tests*/

}
