|  |version| |py-versions| |license| |appveyor|

.. |appveyor| image:: https://img.shields.io/appveyor/ci/austecon/psutil/master.svg?maxAge=3600&label=Windows
    :target: https://ci.appveyor.com/project/austecon/psutil
    :alt: Windows tests (Appveyor)

.. |coverage| image:: https://coveralls.io/repos/github/austecon/psutil/badge.svg?branch=master
    :target: https://coveralls.io/github/austecon/psutil?branch=master
    :alt: Test coverage (coverall.io)

.. |version| image:: https://img.shields.io/pypi/v/psutil-wheels.svg?label=pypi
    :target: https://pypi.org/project/psutil-wheels
    :alt: Latest version

.. |py-versions| image:: https://img.shields.io/pypi/pyversions/psutil-wheels.svg
    :target: https://pypi.org/project/psutil-wheels
    :alt: Supported Python versions

.. |license| image:: https://img.shields.io/pypi/l/psutil.svg
    :target: https://github.com/austecon/psutil/blob/master/LICENSE
    :alt: License

-----

Psutil-wheels
==============
The only reason this repo exists is to supply the necessary python 3.10
cross-platform wheels. It is a direct fork of v5.8.0 https://github.com/giampaolo/psutil
at the time of writing. You are welcome to use this via pypi from
https://pypi.org/project/psutil-wheels/ but there will be no support provided.
99.9% of people should continue using the official psutil. Only use this if
it has a wheel for psutil that you cannot get from the official source.
