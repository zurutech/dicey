Dicey Transfer Format
=====================

The **Dicey Transfer Format *(DTF)*** is a serialisation format used by Dicey interernally to encode the data 
and messages that are sent between a server and its clients.

DTF is a simple **binary** format that strives to be quick to parse, while still being easy to inspect and reason about.

.. warning:: 
    **DTF is NOT intended to be a general purpose serialisation format**, and it is not advisable to use it to exchange 
    data between different systems.
    
    DTF assumes that **both the encoder and decoder run on the same architecture and OS**, 
    which allows it to make certain assumptions about the data that would not be possible in a more general format. 

Packets
~~~~~~~

DTF is based on the concept of **packets**, self contained units of data that can be sent over the Dicey protocol. 

Packets are variable in size, and come in three different types, identified by the *first byte of the packet*:

|packet|

**Bye packets**:
    Bye packets are used to signal the end of a connection. 






.. |packet| image:: ../_static/packet.svg
  :width: 400
  :align: middle
  :alt: Dicey packet overview