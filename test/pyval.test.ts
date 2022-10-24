import { PyObject, pyval } from '../lib';
import { assert } from 'chai';

describe('pyval', () => {
  it('basic pyval', () => {
    const py_array = pyval('list([1, 2, 3])');
    assert.instanceOf(py_array, PyObject);
    assert.deepEqual(py_array.toJS(), [1, 2, 3]);
  });

  it('function', () => {
    const py_fn = pyval('lambda x: (x + 42)');

    assert.instanceOf(py_fn, PyObject);
    assert.isTrue(py_fn.callable);
    assert.strictEqual(py_fn.call(-42).toJS(), 0);

    const js_fn = py_fn.toJS();
    assert.typeOf(js_fn, 'function');
    assert.equal(js_fn(-42).toJS(), 0);
  });

  it('w/ args', () => {
    const py_array = pyval('list([1, x, 3])', { x: 4 });
    assert.instanceOf(py_array, PyObject);
    assert.deepEqual(py_array.toJS(), [1, 4, 3]);
  });

});
