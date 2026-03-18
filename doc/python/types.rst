.. SPDX-License-Identifier: MPL-2.0
.. SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

Core Types
==========

Enumerations
------------

.. autoclass:: osi_utilities.tracefile._types.MessageType
   :members:
   :undoc-members:
   :show-inheritance:

.. autoclass:: osi_utilities.tracefile._types.TraceFileFormat
   :members:
   :undoc-members:
   :show-inheritance:

Data Classes
------------

.. autoclass:: osi_utilities.tracefile._types.ReadResult
   :members:
   :undoc-members:
   :show-inheritance:

.. autoclass:: osi_utilities.tracefile._types.ChannelSpecification
   :members:
   :undoc-members:
   :show-inheritance:

Utility Functions
-----------------

.. autofunction:: osi_utilities.tracefile._types.infer_message_type_from_filename

.. autofunction:: osi_utilities.tracefile._types.get_trace_file_format

.. autofunction:: osi_utilities.tracefile._types.parse_osi_trace_filename
